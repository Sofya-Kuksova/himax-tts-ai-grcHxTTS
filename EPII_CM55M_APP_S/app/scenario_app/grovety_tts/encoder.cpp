#include "encoder.hpp"


EncoderModel::EncoderModel()
    : interpreter(nullptr)
    , block_on_invoke(true)
{
    op_resolver.AddEthosU();
    op_resolver.AddTranspose();
    op_resolver.AddBatchMatMul();
    op_resolver.AddQuantize();
    op_resolver.AddDequantize();
    op_resolver.AddCumSum();
    op_resolver.AddGreater();
    op_resolver.AddCast();
    op_resolver.AddSum();
    op_resolver.AddGather();
    op_resolver.AddRound();
    op_resolver.AddReshape();
}

EncoderModel::~EncoderModel()
{
    if (interpreter)
        delete interpreter;
}

bool EncoderModel::init(const void* model_addr, uint8_t* arena, size_t arena_size)
{
    const tflite::Model* model = tflite::GetModel(model_addr);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERROR] Encoder: Model schema version %ld is not equal "
               "to supported version %d\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    printf("Encoder: Model schema version %ld\n", model->version());

    interpreter = new tflite::MicroInterpreter(model, op_resolver, arena, arena_size);
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("Encoder: Failed to allocate tensors\n");
        return false;
    }
    printf("Encoder: Arena in use : %d bytes\n", interpreter->arena_used_bytes());

    interpreter->SetMicroExternalContext(&block_on_invoke);

    return true;
}

bool EncoderModel::invoke(bool block)
{
    block_on_invoke = block;
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Encoder: Failed to invoke model\n");
        return false;
    }
    return true;
}

bool EncoderModel::set_input(int8_t* data, size_t length)
{
    TfLiteTensor* input = interpreter->input(0);
    if (input->bytes == length) {
        memcpy(input->data.int8, data, length);
        return true;
    }
    return false;
}

void EncoderModel::get_spec_params(size_t* bins, size_t* length)
{
    if (bins && length) {
        TfLiteTensor* spec = interpreter->output(0);
        *bins              = spec->dims->data[2];
        *length            = spec->dims->data[1];
    }
}

bool EncoderModel::get_spec(int8_t* spec_buffer, size_t length)
{
    TfLiteTensor* spec = interpreter->output(0);
    if (spec->bytes == length) {
        memcpy(spec_buffer, spec->data.int8, length);
        return true;
    }
    return false;
}

bool EncoderModel::get_duration(float* duration_buffer, size_t length)
{
    TfLiteTensor* dur = interpreter->output(1);
    if (size_t(dur->dims->data[0]) == length) {
        dequantize(1, dur->data.int8, duration_buffer, length);
        return true;
    }
    return false;
}

void EncoderModel::dequantize(size_t tensor_idx, int8_t* q_data, float* f_data, size_t length)
{
    TfLiteTensor* tensor = interpreter->output(tensor_idx);
    for (size_t i = 0; i < length; i++) {
        f_data[i] = float(q_data[i] - tensor->params.zero_point) * tensor->params.scale;
    }
}
