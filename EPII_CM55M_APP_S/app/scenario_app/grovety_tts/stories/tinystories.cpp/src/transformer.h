#pragma once

#include <cstddef>
#include <cstdint>

#include "ops.h"


struct TransformerWeights
{
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    float* rms_final_weight; // (dim,)
    float* fcr_weights;
    float* fci_weights;

    // token embedding table
    QuantizedTensor embeddings_unencoded; // (vocab_size, dim)
    QuantizedTensor q_tokens; // (vocab_size, dim)
    // weights for matmuls. note dim == n_heads * head_size
    QuantizedTensor wq[MODEL_NUM_LAYERS]; // (layer, dim, n_heads * head_size)
    QuantizedTensor wk[MODEL_NUM_LAYERS]; // (layer, dim, n_kv_heads * head_size)
    QuantizedTensor wv[MODEL_NUM_LAYERS]; // (layer, dim, n_kv_heads * head_size)
    QuantizedTensor wo[MODEL_NUM_LAYERS]; // (layer, n_heads * head_size, dim)
    // weights for ffn
    QuantizedTensor w1[MODEL_NUM_LAYERS]; // (layer, hidden_dim, dim)
    QuantizedTensor w2[MODEL_NUM_LAYERS]; // (layer, dim, hidden_dim)
    QuantizedTensor w3[MODEL_NUM_LAYERS]; // (layer, hidden_dim, dim)
};

struct TransformerState
{
    float x[MODEL_DIM];
    float xb[MODEL_DIM];
    float xb2[MODEL_DIM];
    float hb[MODEL_HIDDEN_DIM];
    float hb2[MODEL_HIDDEN_DIM];
    QuantizedTensor xq;
    QuantizedTensor hq;
    float q[MODEL_DIM];
    float k[MODEL_KV_DIM];
    float v[MODEL_KV_DIM];
    float att[MODEL_NUM_HEADS * MODEL_SEQ_LENGTH];
    float logits[MODEL_VOCAB_SIZE];

    // float key_cache[MODEL_NUM_LAYERS * MODEL_SEQ_LENGTH * MODEL_KV_DIM];
    // float value_cache[MODEL_NUM_LAYERS * MODEL_SEQ_LENGTH * MODEL_KV_DIM];
    float* key_cache;
    float* value_cache;
};


class Transformer
{
public:
    Transformer();
    size_t init(const void* weights, uint8_t* arena, size_t arena_size);

    float* forward(int token, int pos);

private:
    TransformerWeights w;

};