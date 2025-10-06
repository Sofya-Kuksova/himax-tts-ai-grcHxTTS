#include "transformer.h"
#include "fully_connected_weights.h"

#include <cstdio>
#include <cstring>
#include <math.h>


static TransformerState t_state;


Transformer::Transformer()
{
}

size_t Transformer::init(const void* weights, uint8_t* arena, size_t arena_size)
{
    int8_t* weights_ptr = (int8_t*)weights;
    size_t cache_size = sizeof(float) * MODEL_NUM_LAYERS * MODEL_SEQ_LENGTH * MODEL_KV_DIM;
    size_t mem_required = cache_size * 2 + MODEL_DIM + MODEL_HIDDEN_DIM;
    if (mem_required > arena_size) {
        printf("Error: Failed to allocate LLM transformer: not enough memory.\n");
        return 0;
    }

    t_state.xq = QuantizedTensor((int8_t*)arena, 1.0f, MODEL_DIM, 1);
    arena += MODEL_DIM;

    t_state.hq = QuantizedTensor((int8_t*)arena, 1.0f, MODEL_HIDDEN_DIM, 1);
    arena += MODEL_HIDDEN_DIM;

    t_state.key_cache = (float*)arena;
    arena += cache_size;

    t_state.value_cache = (float*)(arena);
    arena += cache_size;

    w.rms_att_weight = (float*)(weights_ptr + rms_att_norm_offset[0]);
    w.rms_ffn_weight = (float*)(weights_ptr + rms_ffn_norm_offset[0]);
    w.rms_final_weight = (float*)(weights_ptr + rms_final_norm_offset[0]);
    w.fcr_weights = (float*)(weights_ptr + fcr_offset[0]);
    w.fci_weights = (float*)(weights_ptr + fci_offset[0]);

    w.q_tokens = QuantizedTensor(
        weights_ptr + embeddings_offsets[0],
        embeddings_scale[0],
        MODEL_VOCAB_SIZE, MODEL_DIM);

    w.embeddings_unencoded = QuantizedTensor(
        weights_ptr + embeddings_without_encoding_offset[0],
        embeddings_scale[0],
        MODEL_VOCAB_SIZE, MODEL_DIM);

    for (int layer = 0; layer < MODEL_NUM_LAYERS; layer++) {
        w.wq[layer] = QuantizedTensor(
            weights_ptr + wq_offsets[layer],
            wq_scale[layer],
            MODEL_NUM_HEADS * MODEL_HEAD_SIZE, MODEL_DIM);

        w.wk[layer] = QuantizedTensor(
            weights_ptr + wk_offsets[layer],
            wk_scale[layer],
            MODEL_NUM_KV_HEADS * MODEL_HEAD_SIZE, MODEL_DIM);

        w.wv[layer] = QuantizedTensor(
            weights_ptr + wv_offsets[layer],
            wv_scale[layer],
            MODEL_NUM_KV_HEADS * MODEL_HEAD_SIZE, MODEL_DIM);

        w.wo[layer] = QuantizedTensor(
            weights_ptr + wo_offsets[layer],
            wo_scale[layer],
            MODEL_DIM, MODEL_NUM_HEADS * MODEL_HEAD_SIZE);

        w.w1[layer] = QuantizedTensor(
            weights_ptr + w1_offsets[layer],
            w1_scale[layer],
            MODEL_HIDDEN_DIM, MODEL_DIM);

        w.w2[layer] = QuantizedTensor(
            weights_ptr + w2_offsets[layer],
            w2_scale[layer],
            MODEL_DIM, MODEL_HIDDEN_DIM);

        w.w3[layer] = QuantizedTensor(
            weights_ptr + w3_offsets[layer],
            w3_scale[layer],
            MODEL_HIDDEN_DIM, MODEL_DIM);
    }

    return mem_required;
}

#define DO_MEASURE(STR, X) { \
started = get_timestamp_us(); \
X; \
ended = get_timestamp_us(); \
printf("%s: %lu us\n", STR, (uint32_t)(ended - started)); \
}

#if 0
#define MEASURE(STR, X) DO_MEASURE(STR, X)
#else
#define MEASURE(STR, X) X;
#endif


#if 0
#define MEASURE2(STR, X) DO_MEASURE(STR, X)
#else
#define MEASURE2(STR, X) X;
#endif


void print_array(const char* name, const float* arr, size_t size)
{
    printf("const float %s[] = {\n", name);
    for (size_t i = 0; i < size; i++) {
        printf("%ff, ", arr[i]);
        if (i % 16 == 15) {
            printf("\n");
        }
    }
    printf("\n};\n");
}

float* Transformer::forward(int token, int pos)
{
    uint64_t started, ended;

    TransformerState* s = &t_state;
    float* x = s->x;
    int kv_mul = MODEL_NUM_HEADS / MODEL_NUM_KV_HEADS; // integer multiplier of the kv sharing in multiquery
    float* fcr = w.fcr_weights + pos * (MODEL_HEAD_SIZE / 2);
    float* fci = w.fci_weights + pos * (MODEL_HEAD_SIZE / 2);

    // copy the token embedding into x
    MEASURE2("dequantize_range",
    dequantize_embeddings(x, w.embeddings_unencoded, token);
    );

    // forward all the layers
    for (int l = 0; l < MODEL_NUM_LAYERS; l++) {
        MEASURE2("Layer",
        // attention rmsnorm
        MEASURE("rmsnorm",
        rmsnorm(s->xb, x, w.rms_att_weight + l * MODEL_DIM, MODEL_DIM);
        );

        // qkv matmuls for this position
        MEASURE("quantize",
        quantize(s->xq, s->xb, MODEL_DIM);
        );

        MEASURE("matmul",
        matmul_ethosu(s->q, s->xq, w.wq[l], "wq", l);
        matmul_ethosu(s->k, s->xq, w.wk[l], "wk", l);
        matmul_ethosu(s->v, s->xq, w.wv[l], "wv", l);
        );

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        MEASURE("RoPE",
            RoPE(s->q, s->q, MODEL_DIM, fcr, fci);
            RoPE(s->k, s->k, MODEL_KV_DIM, fcr, fci);
        );

        // save key,value at this time step (pos) to our kv cache
        int loff = l * MODEL_SEQ_LENGTH * MODEL_KV_DIM; // kv cache layer offset for convenience
        float* key_cache_row = s->key_cache + loff + pos * MODEL_KV_DIM;
        float* value_cache_row = s->value_cache + loff + pos * MODEL_KV_DIM;

        MEASURE("memcpy",
        memcpy(key_cache_row, s->k, MODEL_KV_DIM * sizeof(*key_cache_row));
        memcpy(value_cache_row, s->v, MODEL_KV_DIM * sizeof(*value_cache_row));
        );

        MEASURE("MHA",
        memset(s->att, 0, MODEL_NUM_HEADS * MODEL_SEQ_LENGTH * sizeof(float));
        // multihead attention. iterate over all heads
        memset(s->att, 0, MODEL_NUM_HEADS * MODEL_SEQ_LENGTH * sizeof(*s->att));

        float score = 0.0f;
        for (int h = 0; h < MODEL_NUM_HEADS; h++) {
            // get the query vector for this head
            float* q = s->q + h * MODEL_HEAD_SIZE;
            // attention scores for this head
            float* att = s->att + h * MODEL_SEQ_LENGTH;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = s->key_cache + loff + t * MODEL_KV_DIM + (h / kv_mul) * MODEL_HEAD_SIZE;
                // calculate the attention score as the dot product of q and k
                score = 0;
                for (int i = 0; i < MODEL_HEAD_SIZE; i++) {
                    score += q[i] * k[i];
                }

                score /= sqrtf(MODEL_HEAD_SIZE);
                // save the score to the attention buffer
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float* xb = s->xb + h * MODEL_HEAD_SIZE;
            memset(xb, 0, MODEL_HEAD_SIZE * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = s->value_cache + loff + t * MODEL_KV_DIM + (h / kv_mul) * MODEL_HEAD_SIZE;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < MODEL_HEAD_SIZE; i++) {
                    xb[i] += a * v[i];
                }
            }
        }
        );

        // final matmul to get the output of the attention
        MEASURE("quantize",
        quantize(s->xq, s->xb, MODEL_DIM);
        );

        MEASURE("matmul",
            matmul_ethosu(s->xb2, s->xq, w.wo[l], "wo", l);
        );

        // residual connection back into x
        MEASURE("elementwise_add",
        elementwise_add(x, x, s->xb2, MODEL_DIM);
        );

        // ffn rmsnorm
        MEASURE("rmsnorm",
        rmsnorm(s->xb, x, w.rms_ffn_weight + l * MODEL_DIM, MODEL_DIM);
        );

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        MEASURE("quantize",
        quantize(s->xq, s->xb, MODEL_DIM);
        );

        MEASURE("matmul",
            matmul_ethosu(s->hb, s->xq, w.w1[l], "w1", l);
            matmul_ethosu(s->hb2, s->xq, w.w3[l], "w3", l);
        );

        // SwiGLU non-linearity
        MEASURE("SwiGLU",
        swiglu(s->hb, s->hb, s->hb2, MODEL_HIDDEN_DIM);
        );

        // final matmul to get the output of the ffn
        MEASURE("quantize",
        quantize(s->hq, s->hb, MODEL_HIDDEN_DIM);
        );

        MEASURE("matmul",
            matmul_ethosu(s->xb, s->hq, w.w2[l], "w2", l);
        );

        // residual connection
        MEASURE("elementwise_add",
        elementwise_add(x, x, s->xb, MODEL_DIM);
        );
        );
    }

    // final rmsnorm
    MEASURE2("rmsnorm",
    rmsnorm(x, x, w.rms_final_weight, MODEL_DIM);
    );

    MEASURE2("quantize",
    // classifier into logits
    quantize(s->xq, x, MODEL_DIM);
    );

    MEASURE2("matmul",
    matmul_ethosu(s->logits, s->xq, w.q_tokens);
    );

    return s->logits;
}
