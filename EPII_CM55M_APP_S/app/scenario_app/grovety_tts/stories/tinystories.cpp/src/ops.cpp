#include "ops.h"
#include <ethosu_driver.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <cassert>

#include "ops_ethosu.h"


// ----------------------------------------------------------------------------
// Quantization functions

void dequantize_embeddings(float* x, const QuantizedTensor& qx, int token)
{
    for (size_t i = 0; i < MODEL_DIM; i++) {
        int idx = token * MODEL_DIM + i;
        x[i] = qx.q[idx] * qx.s;
    }
}

void quantize(QuantizedTensor& qx, float* x, size_t n)
{
    const float Q_MAX = 127.0f;

    float wmax = 0.0;
    for (size_t i = 0; i < n; i++) {
        float val = fabs(x[i]);
        if (val > wmax) {
            wmax = val;
        }
    }

    // calculate and write the scaling factor
    float scale = wmax / Q_MAX;
    qx.s = scale;

    // calculate and write the quantized values
    for (size_t i = 0; i < n; i++) {
        float quant_value = x[i] / scale; // scale
        int8_t quantized = (int8_t)round(quant_value); // round and clamp
        qx.q[i] = quantized;
    }
}

// ----------------------------------------------------------------------------
// neural net blocks; the dynamics of the Transformer
void rmsnorm(float* o, float* x, float* weight, size_t size)
{
    // calculate sum of squares
    float ss = 0.0f;
    for (size_t j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (size_t j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void elementwise_add(float* xout, float* a, float* b, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        xout[i] = a[i] + b[i];
    }
}

void elementwise_mul(float* xout, float* a, float* b, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        xout[i] = a[i] * b[i];
    }
}

void softmax(float* x, size_t size)
{
    // find max value (for numerical stability)
    float max_val = x[0];
    for (size_t i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        x[i] = exp(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (size_t i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void swiglu(float* xout, float* x1, float* x2, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        float val = x1[i];
        val = val / (1.0f + exp(-val));
        xout[i] = val * x2[i];
    }
}

void RoPE(float* xout, float* x, size_t size, float* fcr_weights, float* fci_weights)
{
    for (size_t i = 0; i < size; i += 2) {
        float fcr = fcr_weights[(i % MODEL_HEAD_SIZE)/ 2];
        float fci = fci_weights[(i % MODEL_HEAD_SIZE)/ 2];
        float v0 = x[i];
        float v1 = x[i + 1];
        xout[i] = v0 * fcr - v1 * fci;
        xout[i + 1] = v0 * fci + v1 * fcr;
    }
}

SetInferenceParametersFunction get_inference_parameters_function(size_t n, size_t d)
{
    if (d == 288 && n == 288) {
        return set_params_for_fc_d_288_n_288;
    }
    else if (d == 768 && n == 288) {
        return set_params_for_fc_d_768_n_288;
    }
    else if (d == 288 && n == 768) {
        return set_params_for_fc_d_288_n_768;
    }
    else if (d == 4096 && n == 288) {
        return set_params_for_fc_d_4096_n_288;
    }
    else {
        printf("Error: there is no configuration for d = %d, n = %d\n", d , n);
        std::exit(-1);
    }
}

void *get_cmd_data(const std::string& w_name, size_t layer)
{
    if (w_name == "w1") {
        return w1_cmd_datas[layer];
    }
    else if (w_name == "w2") {
        return w2_cmd_datas[layer];
    }
    else if (w_name == "w3") {
        return w3_cmd_datas[layer];
    }
    else if (w_name == "wk") {
        return wk_cmd_datas[layer];
    }
    else if (w_name == "wo") {
        return wo_cmd_datas[layer];
    }
    else if (w_name == "wq") {
        return wq_cmd_datas[layer];
    }
    else if (w_name == "wv") {
        return wv_cmd_datas[layer];
    }
    else {
        return embeddings_cmd_data;
    }
}

void matmul_ethosu(float *xout, const QuantizedTensor& x, const QuantizedTensor& w, const std::string& w_name, size_t layer)
{
    // W (d,n) @ x (n,) -> xout (d,)
    assert(x.rows == w.cols);
    assert(x.cols == 1);

    size_t cmd_data_size = 0;
    void *cmd_data = 0;
    size_t base_addrs_size[6] = {0};
    uint64_t base_addrs[6] = {0};
    void* ofm;

    void *cmd_data_src = get_cmd_data(w_name, layer);
    SetInferenceParametersFunction set_inference_params = get_inference_parameters_function(w.cols, w.rows);
    set_inference_params(&cmd_data, cmd_data_src, &cmd_data_size, base_addrs, base_addrs_size, x.q, w.q, &ofm);

    struct ethosu_driver *driver = ethosu_reserve_driver();
    if (! driver) {
        printf("Error creating driver\n");
        return;
    }

    int32_t result = ethosu_invoke(driver, cmd_data, cmd_data_size, base_addrs, base_addrs_size, 6);
    if (result != 0) {
        printf("Error running NPU\n");
        goto release_driver;
    }

    for (size_t i = 0; i < w.rows; i++) {
        xout[i] = ((int32_t*)ofm)[i] * w.s * x.s;
    }

release_driver:
    ethosu_release_driver(driver);
}
