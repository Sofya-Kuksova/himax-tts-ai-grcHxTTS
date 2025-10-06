#pragma once

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

class EncoderModel
{
    tflite::MicroInterpreter* interpreter;
    bool block_on_invoke;
    tflite::MicroMutableOpResolver<12> op_resolver;

public:
    EncoderModel();
    ~EncoderModel();

    bool init(const void* model_addr, uint8_t* arena, size_t arena_size);
    bool invoke(bool block = true);
    bool set_input(int8_t* data, size_t length);
    void get_spec_params(size_t* bins, size_t* length);
    bool get_spec(int8_t* spec_buffer, size_t length);
    bool get_duration(float* duration_buffer, size_t length);
    void dequantize(size_t tensor_idx, int8_t* q_data, float* f_data, size_t length);
};
