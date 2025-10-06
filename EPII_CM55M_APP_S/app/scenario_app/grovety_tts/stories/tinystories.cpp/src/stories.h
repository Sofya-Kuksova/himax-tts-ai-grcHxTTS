#pragma once

#include <functional>
#include <string>
#include <cstdint>

#include "sampler.h"
#include "tokenizer.h"
#include "transformer.h"

class TinyStoriesModel
{
    Transformer transformer;
    Sampler sampler;
    Tokenizer tokenizer;

    int steps = 128;
    float temperature = 1.0f;
    float topp = 0.9f;

public:
    typedef std::function<void(std::string)> TokenReadyCallback;

    TinyStoriesModel();
    ~TinyStoriesModel();

    bool init(const void* weights, uint8_t* arena, size_t arena_size);

    /// @brief Set internal model parameters
    /// @param steps number of steps to run for
    /// @param temperature 0.0 = greedy deterministic. 1.0 = original. don't set higher
    /// @param topp top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    void setParams(int steps, float temperature, float topp);

    /// @brief Run tiny version of the LLama2 model trained on TinyStories dataset
    /// @param prompt prompt string
    /// @param seed seed rng with time by default
    std::string invoke(const std::string& prompt = std::string(), uint32_t seed = 0, TokenReadyCallback callback = TokenReadyCallback());
    std::string invoke(const std::string& prompt, TokenReadyCallback callback) {
        return invoke(prompt, 0, callback);
    }
};
