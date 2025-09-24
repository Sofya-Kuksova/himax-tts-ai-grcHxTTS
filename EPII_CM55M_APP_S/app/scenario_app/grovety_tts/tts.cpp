#include "board.h"
#include "time_calculation.h"

#include "ethosu_driver.h"

#include "common.h"
#include "grovety_tts.hpp"
#include "i2c_protocol.h"
#include "i2cs.h"
#include "text_proc.h"
#include "tts.hpp"

#define RS_RATIO         (16000. / 22050.)
#define RS_IN_FRAME_LEN  441
#define RS_OUT_FRAME_LEN 320

template <size_t M, size_t N> void gather_2d(int8_t* output, const int8_t* data, const int32_t* indices)
{
    for (size_t i = 0; i < N; i++) {
        assert(size_t(indices[i]) < M);
        const int8_t* src = &data[indices[i] * N];
        int8_t* dst       = &output[i * N];
        memcpy(dst, src, N);
    }
    return;
}

static void sentence_callback(const char* sentence_start, size_t sentence_length, void* user_data)
{
    // printf("sentence_length=%u: %.*s\n", sentence_length, (int)sentence_length, sentence_start);

    std::vector<char*>* sentences = static_cast<std::vector<char*>*>(user_data);

    // create null terminated string
    char* temp = (char*)malloc(sentence_length + 1);
    memcpy(temp, sentence_start, sentence_length);
    temp[sentence_length] = '\0';
    sentences->push_back(temp);
}

TTS::TTS() noexcept
    : phonemes_table(PHONEMES_TABLE_BASE_ADDR, PHONEMES_TABLE_SIZE)
    , embeddings(reinterpret_cast<const __embeddings_tiny_qnn_dtype*>(EMBEDDINGS_BASE_ADDR))
{
    memset(&phonemes, 0, sizeof(phonemes));
    memset(&duration, 0, sizeof(duration));
    memset(&model_input, 0, sizeof(model_input));
    sentences.reserve(8);
}

TTS::~TTS() noexcept
{
    free(spec_buffer);
    free(decoder_input);
    free(audio_buffer);
    src_delete(resampler_state);

    delete encoder;
    delete decoder;
}

bool TTS::init(uint8_t* arena, size_t arena_size)
{
    const auto small_pause_item = phonemes_table.LookupByKey("@sp");
    if (! small_pause_item.data) {
        printf("Unable to find pause item in phonemes table\n");
        return false;
    }
    SMALL_PAUSE_ID = small_pause_item.data[0];

    const auto sil_item = phonemes_table.LookupByKey("@sil");
    if (! sil_item.data) {
        printf("Unable to find sil item in phonemes table\n");
        return false;
    }
    SIL_ID = sil_item.data[0];

    uint32_t t1 = get_timestamp_ms();
    encoder = new EncoderModel();
    if (! encoder->init(MODEL_ENCODER_TINY_BASE_ADDR, arena, ENCODER_ARENA_SIZE))
        return false;
    printf("Encoder: Model initialized: %ld ms\n", get_timestamp_ms() - t1);

    size_t spec_bins = 0;
    encoder->get_spec_params(&spec_bins, &max_spec_length);
    printf("spec: bins=%u, length=%u\n", spec_bins, max_spec_length);
    spec_size   = spec_bins * max_spec_length;
    spec_buffer = (int8_t*)calloc(spec_size, sizeof(int8_t));
    if (! spec_buffer) {
        return false;
    }

    t1 = get_timestamp_ms();
    decoder = new DecoderModel();
    if (! decoder->init(MODEL_DECODER_I16_32_BASE_ADDR, arena + ENCODER_ARENA_SIZE, DECODER_ARENA_SIZE))
        return false;
    printf("Decoder: Model initialized: %ld ms\n", get_timestamp_ms() - t1);

    decoder->get_audio_params(&spec_chunk_size, &audio_chunk_size);
    printf("spec_chunk_size=%d, audio_chunk_size=%d\n", spec_chunk_size, audio_chunk_size);
    decoder_input_chunk_size = spec_bins * spec_chunk_size;

    decoder_input = (float*)calloc(decoder_input_chunk_size, sizeof(float));
    if (! decoder_input) {
        return false;
    }
    audio_buffer = (int16_t*)calloc(audio_chunk_size, sizeof(int16_t));
    if (! audio_buffer) {
        return false;
    }

    int err;
    resampler_state = src_new(4, 1, &err);
    printf("resampler: %s &resampler_state=%p\n", src_strerror(err), resampler_state);
    if (! resampler_state) {
        return false;
    }
    err = src_set_ratio(resampler_state, RS_RATIO);
    printf("resampler: %s\n", src_strerror(err));

    return true;
}

int TTS::run(const char* text)
{
    int ret = 0;
    uint32_t t1, t2, preproc_time = 0, audio_gen_time = 0;
    t1 = get_timestamp_ms();

    char* norm_text = normalize_numbers((const char*)text);
    size_t i        = 0;
    while (norm_text[i] != '\0') {
        norm_text[i] = tolower((unsigned char)norm_text[i]);
        i++;
    }
    // printf("Normalized text: %s\n", norm_text);

    split_by_sentences(norm_text, sentence_callback, &sentences);

    free(norm_text);

    preproc_time += get_timestamp_ms() - t1;

    for (size_t sent_idx = 0; sent_idx < sentences.size(); sent_idx++) {
        printf("tokens:\n");
        const char* p = sentences[sent_idx];
        while (*p != '\0') {
            t1               = get_timestamp_ms();
            size_t token_idx = 0;
            while (token_idx < SEQ_LEN) {
                // Skip NUL bytes
                if (! *p)
                    break;

                // Determine if current character is a delimiter
                int is_delim = isspace((unsigned char)*p) || (ispunct((unsigned char)*p) && *p != '\'');

                if (! is_delim) {
                    // Start of a word token
                    const char* start = p;
                    while (*p && ! isspace((unsigned char)*p) && ! (ispunct((unsigned char)*p) && *p != '\'')) {
                        p++;
                    }

                    size_t len  = p - start;
                    char* token = (char*)malloc(len + 1);
                    if (token) {
                        strncpy(token, start, len);
                        token[len] = '\0';

                        const auto found_value = phonemes_table.LookupByKey(token);
                        if (found_value.data) {
                            if (token_idx + found_value.len >= MAX_ACTUAL_SEQ_LEN) {
                                // stop prematurely if we exceed the input size
                                p = start;
                                break;
                            }
                            for (int32_t i = 0; i < found_value.len; i++) {
                                phonemes[token_idx++] = found_value.data[i];
                            }
                        }

                        printf("<%s> ", token);

                        free(token);
                    }
                } else {
                    // Delimiter token
                    if (isspace((unsigned char)*p)) {
                        // Collapse whitespace to a single space
                        while (isspace((unsigned char)*p)) {
                            p++;
                        }
                        //   printf("< > "); // Output single space
                    } else {
                        // Output punctuation as-is
                        char c = *p;
                        p++;
                        if (c != '\"' && c != '-') {
                            printf("<%c> ", c);
                            phonemes[token_idx++] = SMALL_PAUSE_ID;
                        }
                    }
                }
            }
            printf("\n");

            const size_t speech_length = token_idx;

            if (token_idx < SEQ_LEN) {
                phonemes[token_idx++] = SIL_ID;
                while (token_idx < SEQ_LEN) {
                    phonemes[token_idx++] = SMALL_PAUSE_ID;
                }
            }

            // printf("phonemes:\n");
            // for (size_t i = 0; i < SEQ_LEN; i++) {
            //     printf("%ld ", phonemes[i]);
            // }
            // printf("\n");

            gather_2d<__EMBEDDINGS_TINY_QNN_HEIGHT, __EMBEDDINGS_TINY_QNN_WIDTH>(model_input, embeddings, phonemes);

            preproc_time += get_timestamp_ms() - t1;

            printf("Encoder start\n");
            t1 = get_timestamp_ms();
            encoder->set_input(model_input, SEQ_LEN * SEQ_LEN);
            encoder->invoke();
            encoder->get_spec(spec_buffer, spec_size);
            encoder->get_duration(duration, SEQ_LEN);
            size_t spec_length = 0;
            for (size_t i = 0; i < speech_length; i++) {
                spec_length += size_t(round(duration[i]));
            }

            t2 = get_timestamp_ms();

            audio_gen_time += t2 - t1;

            printf("Spec generation time: %ld ms\n", t2 - t1);

            spec_length = MIN(spec_length, max_spec_length);

            // printf("spec_length=%u\n", spec_length);

            const size_t frame_num = (spec_length + spec_chunk_size - 1) / spec_chunk_size;
            size_t rem_samples =
                (spec_length % spec_chunk_size) * 256; // multiply by number of samples in 1 spectrum value
            if (rem_samples == 0) {
                rem_samples = audio_chunk_size;
            }

            printf("Decoder start\n");
            t1 = get_timestamp_ms();
            for (size_t idx = 0; idx <= frame_num; idx++) {
                if (idx > 0) {
                    ethosu_wait(&g_ethosu_drv, true);
                    ethosu_release_driver(&g_ethosu_drv);
                    decoder->get_audio(audio_buffer, audio_chunk_size);
                }

                // skip last
                if (idx != frame_num) {
                    int8_t* spec_chunk_ptr = &spec_buffer[decoder_input_chunk_size * idx];
                    encoder->dequantize(0, spec_chunk_ptr, decoder_input, decoder_input_chunk_size);
                    decoder->set_input(decoder_input, decoder_input_chunk_size);
                    decoder->invoke(false);
                }

                if (idx > 0) {
                    size_t rs_output_samples =
                        downsample_audio(audio_buffer, idx == frame_num ? rem_samples : audio_chunk_size);
#ifdef EXCHANGE_OVER_I2C
                    if (i2c_send_audio(audio_buffer, rs_output_samples, idx - 1, frame_num) != 0) {
                        // unable to send, so cancel everything
                        ethosu_wait(&g_ethosu_drv, true);
                        ethosu_release_driver(&g_ethosu_drv);
                        ret = -1;
                        goto CLEANUP;
                    }
#endif // EXCHANGE_OVER_I2C
                }
            }
            t2 = get_timestamp_ms();
            audio_gen_time += t2 - t1;
            printf("audio response time: %ld ms\n", t2 - t1);
        }

        // extra pause after sentence
        board_delay_ms(200);
    }

#ifdef EXCHANGE_OVER_I2C
    i2c_send_packet(ST_EOT, ERR_NONE, TYPE_NONE, 0, 0, 0, 0);
    if (i2cs_wait_tx_done() == 0) {
        printf("audio tx timeout\n");
        return -1;
    }
#endif // EXCHANGE_OVER_I2C

    printf("total preproc time: %ld ms\n", preproc_time);
    printf("total audio gen time: %ld ms\n", audio_gen_time);

CLEANUP:
    for (char* sentence : sentences) {
        free(sentence);
        sentence = NULL;
    }
    sentences.clear();
    return ret;
}

size_t TTS::downsample_audio(int16_t* audio_buffer, size_t samples_num)
{
    static float rs_data_in[RS_IN_FRAME_LEN];
    static float rs_data_out[RS_OUT_FRAME_LEN];

    size_t rs_input_samples_count  = 0;
    size_t rs_output_samlpes_count = 0;

    SRC_DATA src_data;
    memset(&src_data, 0, sizeof(SRC_DATA));
    src_data.src_ratio = RS_RATIO;

    src_data.data_out      = rs_data_out;
    src_data.output_frames = RS_OUT_FRAME_LEN;
    src_data.end_of_input  = 0;

    while (1) {
        if (src_data.input_frames == 0) {
            size_t j;
            for (j = 0; (j < RS_IN_FRAME_LEN) && (rs_input_samples_count < samples_num); j++) {
                rs_data_in[j] = (float)((float)audio_buffer[rs_input_samples_count++] / INT16_MAX);
            }
            src_data.data_in      = rs_data_in;
            src_data.input_frames = j;
            if (src_data.input_frames < RS_IN_FRAME_LEN) {
                src_data.end_of_input = 1;
            }
        }

        int err = src_process(resampler_state, &src_data);
        if (err) {
            printf("err: %s\n", src_strerror(err));
        }
        // printf("input_frames_used=%ld, output_frames_gen=%ld, "
        //        "end_of_input=%d\n",
        //        src_data.input_frames_used, src_data.output_frames_gen, src_data.end_of_input);

        if (src_data.end_of_input && src_data.output_frames_gen == 0) {
            break;
        }

        for (long int j = 0; j < src_data.output_frames_gen; j++) {
            const int16_t output_pcm                = (int16_t)(rs_data_out[j] * INT16_MAX);
            audio_buffer[rs_output_samlpes_count++] = output_pcm;
        }

        src_data.data_in += src_data.input_frames_used;
        src_data.input_frames -= src_data.input_frames_used;
    }
    return rs_output_samlpes_count;
}
