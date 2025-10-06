#pragma once
#include <stddef.h>

char* normalize_numbers(const char* text);

typedef void (*sentence_callback_t)(const char* sentence_start, size_t sentence_length, void* user_data);

void split_by_sentences(const char* text, sentence_callback_t callback, void* user_data);
