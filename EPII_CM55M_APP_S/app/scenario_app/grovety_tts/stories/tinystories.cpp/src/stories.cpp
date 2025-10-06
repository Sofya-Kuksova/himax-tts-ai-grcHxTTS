/* Inference for Llama-2 Transformer model in pure C, int8 quantized forward pass. */

#include <math.h>
#include <time.h>
#include <cstring>

#include "sampler.h"
#include "stories.h"
#include "tokenizer.h"
#include "transformer.h"

#ifdef MODEL_ENABLE_NUMERIC_TEST
extern bool test_model_numeric(float* logits, float eps = 0.001f);
#endif // MODEL_ENABLE_NUMERIC_TEST


TinyStoriesModel::TinyStoriesModel()
{
}

TinyStoriesModel::~TinyStoriesModel()
{
}

inline size_t align_to(size_t addr, size_t to)
{
    if (addr % to != 0)
        return (addr / to + 1) * to;
    return addr;
}

void TinyStoriesModel::setParams(int steps, float temperature, float topp)
{
    if (steps < 0 || steps > MODEL_SEQ_LENGTH) {
        steps = MODEL_SEQ_LENGTH; // override to ~max length
    }
    this->steps = steps;

    if (temperature < 0.0f) {
        temperature = 1.0f;
    }
    this->temperature = temperature;

    if (topp < 0.0f || topp > 1.0f) {
        topp = 0.9f;
    }
    this->topp = topp;
}

bool TinyStoriesModel::init(const void* weights, uint8_t* arena, size_t arena_size)
{
    uint8_t *arena_start = arena;

    size_t arena_size_needed = transformer.init(weights, arena, arena_size);
    if (arena_size_needed == 0) {
        return false;
    }
    arena_size_needed = align_to(arena_size_needed, 16);
    printf("Transformer arena: %d kB\n", arena_size_needed / 1024);
    arena_size -= arena_size_needed;
    arena += arena_size_needed;

    arena_size_needed = sampler.init(arena, arena_size);
    if (arena_size_needed == 0) {
        return false;
    }
    arena_size_needed = align_to(arena_size_needed, 16);
    printf("Sampler arena: %d kB\n", arena_size_needed / 1024);
    arena_size -= arena_size_needed;
    arena += arena_size_needed;

    arena_size_needed = tokenizer.init(arena, arena_size);
    if (arena_size_needed == 0) {
        return false;
    }
    arena_size_needed = align_to(arena_size_needed, 16);
    printf("Tokenizer arena: %d kB\n", arena_size_needed / 1024);
    arena_size -= arena_size_needed;
    arena += arena_size_needed;

    printf("Total arena size: %d kB\n", (arena - arena_start) / 1024);

#ifdef MODEL_ENABLE_NUMERIC_TEST
    float* logits = transformer.forward(42, 0);
    test_model_numeric(logits);
#endif // MODEL_ENABLE_NUMERIC_TEST

    return true;
}


bool is_safe_for_print(const char* piece)
{
    // piece might be a raw byte token, and we only want to print printable chars or whitespace
    // because some of the other bytes can be various control codes, backspace, etc.
    if (piece == nullptr) {
        printf("NULL\n");
        return false;
    }
    if (piece[0] == '\0') {
        return false;
    }
    if (piece[1] == '\0') {
        unsigned char byte_val = piece[0];
        if (!(isprint(byte_val) || isspace(byte_val))) {
            return false; // bad byte, don't print it
        }
    }
    return true;
}

std::string TinyStoriesModel::invoke(const std::string& prompt, uint32_t seed, TokenReadyCallback callback)
{
    sampler.temperature = temperature;
    sampler.topp = topp;
    sampler.rng_state = seed;

    // encode the (string) prompt into tokens sequence
    std::vector<int> prompt_tokens = tokenizer.encode(prompt, true, false);
    if (prompt_tokens.size() < 1) {
        fprintf(stderr, "something is wrong, expected at least 1 prompt token\n");
        return std::string();
    }

    // start the main loop
    int next; // will store the next token in the sequence
    int token = prompt_tokens[0]; // kick off with the first token in the prompt
    std::string result;
    for (int pos = 0; pos < steps; pos++) {
        // forward the transformer to get logits for the next token
        float* logits = transformer.forward(token, pos);
        // advance the state state machine
        if (pos < (int)prompt_tokens.size() - 1) {
            // if we are still processing the input prompt, force the next prompt token
            next = prompt_tokens[pos + 1];
        } else {
            // otherwise sample the next token from the logits
            next = sampler.sample(logits);
        }

        // data-dependent terminating condition: the BOS (=1) token delimits sequences
        if (next == 1) {
            break;
        }

        // print the token as string, decode it with the Tokenizer object
        const char* piece = tokenizer.decode(token, next);
        if (is_safe_for_print(piece)) {
            auto t = std::string(piece);
            if (callback) {
                callback(t);
            }
            result += t;
        }

        token = next;
    }
    return result;
}
