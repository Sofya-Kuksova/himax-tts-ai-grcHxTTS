#pragma once

#include <cstdint>

// struct used when sorting probabilities during top-p sampling
struct ProbIndex {
    float prob;
    int index;
};

// The Sampler, which takes logits and returns a sampled token
// sampling can be done in a few ways: greedy argmax, sampling, top-p sampling
class Sampler {
public:
    Sampler();
    size_t init(uint8_t* arena, size_t arena_size);

    int sample(float* logits);

    float temperature;
    float topp;
    uint32_t rng_state;
    
private:
    ProbIndex* probindex; // buffer used in top-p sampling
};
