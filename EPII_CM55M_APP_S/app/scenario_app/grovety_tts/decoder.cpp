#include "decoder.hpp"


DecoderModel::DecoderModel()
    : interpreter(nullptr)
    , block_on_invoke(true)
{
    op_resolver.AddEthosU();
}

DecoderModel::~DecoderModel()
{
    if (interpreter)
        delete interpreter;
}

bool DecoderModel::init(const void* model_addr, uint8_t* arena, size_t arena_size)
{
    const tflite::Model* model = tflite::GetModel(model_addr);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERROR] Decoder: Model schema version %ld is not equal "
               "to supported version %d\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    printf("Decoder: Model schema version %ld\n", model->version());

    interpreter = new tflite::MicroInterpreter(model, op_resolver, arena, arena_size);
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("Decoder: Failed to allocate tensors\n");
        return false;
    }
    printf("Decoder: Arena in use : %d bytes\n", interpreter->arena_used_bytes());

    interpreter->SetMicroExternalContext(&block_on_invoke);

    return true;
}

bool DecoderModel::invoke(bool block)
{
    block_on_invoke = block;
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Decoder: Failed to invoke model\n");
        return false;
    }
    return true;
}

void DecoderModel::get_audio_params(size_t* input_chunk_size, size_t* output_chunk_size)
{
    if (input_chunk_size && output_chunk_size) {
        TfLiteTensor* spec  = interpreter->input(0);
        TfLiteTensor* audio = interpreter->output(0);
        *input_chunk_size   = spec->dims->data[1];
        *output_chunk_size  = audio->dims->data[0];
    }
}

bool DecoderModel::set_input(float* spec, size_t length)
{
    TfLiteTensor* input = interpreter->input(0);
    if (size_t(input->dims->data[1] * input->dims->data[2]) == length) {
        for (size_t i = 0; i < length; i++) {
            input->data.i16[i] = (int16_t)(spec[i] / input->params.scale + input->params.zero_point);
        }
        return true;
    }
    return false;
}

bool DecoderModel::get_audio(int16_t* audio, size_t length)
{
    TfLiteTensor* tensor = interpreter->output(0);
    if (size_t(tensor->dims->data[0]) == length) {
        for (size_t i = 0; i < length; i++) {
            float dequant_value = float(tensor->data.i16[i] - tensor->params.zero_point) * tensor->params.scale;
            audio[i]            = int16_t(dequant_value * INT16_MAX);
        }
        return true;
    }
    return false;
}
