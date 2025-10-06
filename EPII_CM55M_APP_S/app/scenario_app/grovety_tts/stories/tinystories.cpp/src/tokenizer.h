#pragma once

#include <string>
#include <vector>
#include <cstdint>

// The Byte Pair Encoding (BPE) Tokenizer that translates strings <-> tokens

struct TokenIndex
{
    const char* str;
    int id;
};

class Tokenizer
{
public:
    Tokenizer();
    size_t init(uint8_t* arena, size_t arena_size);

    const char* decode(int prev_token, int token);
    std::vector<int> encode(const std::string& text, bool bos, bool eos);

private:
    const char** vocab;
    const float* vocab_scores;
    const unsigned char* byte_pieces; // stores all single-byte strings
    TokenIndex* sorted_vocab;
};
