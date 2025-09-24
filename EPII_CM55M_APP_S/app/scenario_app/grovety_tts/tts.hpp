#pragma once

#include "decoder.hpp"
#include "embeddings_tiny_qnn.h"
#include "encoder.hpp"
#include "memory_map.h"
#include "phonemes_table.h"
#include "samplerate.h"

#define SEQ_LEN __EMBEDDINGS_TINY_QNN_WIDTH
// 70 tokens is  ~6s
#define MAX_ACTUAL_SEQ_LEN 70

class TTS
{
    EncoderModel* encoder;
    DecoderModel* decoder;
    PhonemesTable phonemes_table;
    int32_t phonemes[SEQ_LEN];
    float duration[SEQ_LEN];
    __embeddings_tiny_qnn_dtype model_input[__EMBEDDINGS_TINY_QNN_WIDTH * __EMBEDDINGS_TINY_QNN_WIDTH];
    const __embeddings_tiny_qnn_dtype* embeddings;

    int8_t* spec_buffer;
    float* decoder_input;
    int16_t* audio_buffer;

    size_t max_spec_length;
    size_t spec_size;
    size_t spec_chunk_size;
    size_t audio_chunk_size;
    size_t decoder_input_chunk_size;

    uint8_t SMALL_PAUSE_ID;
    uint8_t SIL_ID;
    std::vector<char*> sentences;
    SRC_STATE* resampler_state;

    size_t downsample_audio(int16_t* audio_buffer, size_t samples_num);

public:
    TTS();
    ~TTS();
    bool init(uint8_t* arena, size_t arena_size);
    int run(const char* text);
};
