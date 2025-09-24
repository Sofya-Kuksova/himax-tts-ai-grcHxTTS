#pragma once

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

class DecoderModel
{
    tflite::MicroInterpreter* interpreter;
    bool block_on_invoke;
    tflite::MicroMutableOpResolver<1> op_resolver;

public:
    DecoderModel();
    ~DecoderModel();

    bool init(const void* model_addr, uint8_t* arena, size_t arena_size);
    bool invoke(bool block = true);
    void get_audio_params(size_t* input_chunk_size, size_t* output_chunk_size);
    bool set_input(float* spec, size_t length);
    bool get_audio(int16_t* audio, size_t length);
};
