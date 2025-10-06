#pragma once

#include <functional>
#include <string>
#include <cstdint>

#include "model.h"


struct QuantizedTensor
{
    int8_t* q;      // quantized values
    float s;       // scaling factors
    size_t rows;
    size_t cols;

    QuantizedTensor()
        : q(nullptr), s(1.0f), rows(0), cols(0)
    {
    }

    QuantizedTensor(int8_t* q, float s, size_t rows, size_t cols)
        : q(q), s(s), rows(rows), cols(cols)
    {
    }
};

class u55_op
{
    uint8_t* cmd = nullptr;
    size_t cmd_len = 0;

    size_t arena_size = 0;

    bool imf_in_arena = true;
    size_t ifm_offt = 0;

    bool ofm_in_arena = true;
    size_t ofm_offt = 0;

};


using SetInferenceParametersFunction = std::function<void(void **cmd_data_dst, void *cmd_data_src, size_t *cms_data_size, uint64_t *base_addrs, size_t *base_addrs_size, void *ifm1, void *weight, void **ofm)>;

void dequantize_embeddings(float* x, const QuantizedTensor& qx, int token);
void quantize(QuantizedTensor& qx, float* x, size_t n);
void rmsnorm(float* o, float* x, float* weight, size_t size);
void elementwise_add(float* xout, float* a, float* b, size_t size);
void elementwise_mul(float* xout, float* a, float* b, size_t size);
void softmax(float* x, size_t size);
void RoPE(float* xout, float* x, size_t size, float* fcr_weights, float* fci_weights);
void swiglu(float* xout, float* x1, float* x2, size_t size);
void matmul_ethosu(float* xout, const QuantizedTensor& x, const QuantizedTensor& w, const std::string& w_name="", size_t layer=0);
SetInferenceParametersFunction get_inference_parameters_function(size_t n, size_t d);
void *get_cmd_data(const std::string& w_name, size_t layer);
void *get_weights_data(const std::string& w_name, size_t layer);
