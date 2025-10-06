#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text_proc.h"

// Word mappings
static const char* ONES[] = {"zero",     "one",     "two",     "three",     "four",     "five",    "six",
                             "seven",    "eight",   "nine",    "ten",       "eleven",   "twelve",  "thirteen",
                             "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};

static const char* TENS[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};

static void convert_hundreds(int num, char* buffer, size_t size)
{
    if (num >= 100) {
        snprintf(buffer, size, "%s hundred", ONES[num / 100]);
        num %= 100;
        if (num > 0)
            strncat(buffer, " ", size - strlen(buffer) - 1);
    }

    if (num >= 20) {
        strncat(buffer, TENS[num / 10], size - strlen(buffer) - 1);
        if (num % 10 > 0) {
            strncat(buffer, "-", size - strlen(buffer) - 1);
            strncat(buffer, ONES[num % 10], size - strlen(buffer) - 1);
        }
    } else if (num > 0) {
        strncat(buffer, ONES[num], size - strlen(buffer) - 1);
    }
}

static void number_to_words(int num, char* buffer, size_t size)
{
    if (num == 0) {
        strncpy(buffer, "zero", size);
        return;
    }

    buffer[0] = '\0';

    int millions  = num / 1000000;
    int remainder = num % 1000000;
    int thousands = remainder / 1000;
    int hundreds  = remainder % 1000;

    if (millions > 0) {
        char temp[128] = {0};
        convert_hundreds(millions, temp, sizeof(temp));
        snprintf(buffer, size, "%s million", temp);
    }

    if (thousands > 0) {
        if (buffer[0] != '\0') {
            strncat(buffer, " ", size - strlen(buffer) - 1);
        }
        char temp[128] = {0};
        convert_hundreds(thousands, temp, sizeof(temp));
        strncat(buffer, temp, size - strlen(buffer) - 1);
        strncat(buffer, " thousand", size - strlen(buffer) - 1);
    }

    if (hundreds > 0) {
        if (buffer[0] != '\0') {
            strncat(buffer, " ", size - strlen(buffer) - 1);
        }
        convert_hundreds(hundreds, buffer + strlen(buffer), size - strlen(buffer));
    }
}

static int parse_dollar(const char* str, int pos, char* out, size_t size)
{
    if (str[pos] != '$')
        return 0;

    int number_pos  = pos + 1;
    int i           = number_pos;
    int digit_count = 0;

    // Parse integer part (with optional commas)
    while (1) {
        if (isdigit(str[i])) {
            digit_count++;
            i++;
        } else if ((str[i] == ',' || str[i] == '.') && digit_count > 0 && isdigit(str[i + 1])) {
            i++;
            digit_count = 0;
        } else {
            break;
        }
    }

    if (i == number_pos)
        return 0;

    // Extract full number
    char num_str[128] = {0};
    int j             = 0;
    for (int k = number_pos; k < i; k++) {
        num_str[j++] = str[k];
    }
    num_str[j] = '\0';

    // Remove commas
    char clean_num[128] = {0};
    j                   = 0;
    for (size_t k = 0; k < strlen(num_str); k++) {
        if (num_str[k] != ',')
            clean_num[j++] = num_str[k];
    }
    clean_num[j] = '\0';

    int dollars = 0, cents = 0;
    // Split into integer and fractional parts
    char* dot = strchr(clean_num, '.');
    if (dot) {
        *dot    = '\0';
        dollars = atoi(clean_num);
        cents   = atoi(dot + 1);
    } else {
        dollars = atoi(clean_num);
    }

    char dwords[128] = "", cwords[128] = "";

    if (dollars > 0)
        number_to_words(dollars, dwords, sizeof(dwords));
    if (cents > 0)
        number_to_words(cents, cwords, sizeof(cwords));

    const char* dunit = (dollars == 1) ? "dollar" : "dollars";
    const char* cunit = (cents == 1) ? "cent" : "cents";

    if (dollars && cents) {
        snprintf(out, size, "%s %s, %s %s", dwords, dunit, cwords, cunit);
    } else if (dollars) {
        snprintf(out, size, "%s %s", dwords, dunit);
    } else if (cents) {
        snprintf(out, size, "%s %s", cwords, cunit);
    } else {
        strcpy(out, "zero dollars");
    }

    return i - pos;
}

static int parse_ordinal(const char* str, int pos, char* out, size_t size)
{
    if (! isdigit(str[pos]))
        return 0;

    int i = pos;
    char num_str[32];
    int j = 0;

    while (isdigit(str[i]))
        num_str[j++] = str[i++];
    num_str[j] = '\0';
    int num    = atoi(num_str);

    if (strncmp(str + i, "st", 2) == 0 || strncmp(str + i, "nd", 2) == 0 || strncmp(str + i, "rd", 2) == 0 ||
        strncmp(str + i, "th", 2) == 0) {

        switch (num) {
        case 1:
            strcpy(out, "first");
            break;
        case 2:
            strcpy(out, "second");
            break;
        case 3:
            strcpy(out, "third");
            break;
        case 4:
            strcpy(out, "fourth");
            break;
        case 5:
            strcpy(out, "fifth");
            break;
        case 8:
            strcpy(out, "eighth");
            break;
        case 9:
            strcpy(out, "ninth");
            break;
        case 10:
            strcpy(out, "tenth");
            break;
        case 11:
            strcpy(out, "eleventh");
            break;
        case 12:
            strcpy(out, "twelfth");
            break;
        default:
            number_to_words(num, out, size);
            strncat(out, "th", size - strlen(out) - 1);
            break;
        }
        return i - pos + 2;
    }
    return 0;
}

static int parse_decimal(const char* str, int pos, char* out, size_t size)
{
    int i           = pos;
    int digit_count = 0;

    // Parse integer part (with optional commas)
    while (1) {
        if (isdigit(str[i])) {
            digit_count++;
            i++;
        } else if (str[i] == ',' && digit_count > 0 && isdigit(str[i + 1])) {
            i++;
            digit_count = 0;
        } else {
            break;
        }
    }

    if (i == pos || str[i] != '.')
        return 0; // No valid decimal start

    // Now parse decimal part
    i++; // Skip the '.'
    int decimal_start = i;
    while (isdigit(str[i]))
        i++;

    if (i == decimal_start)
        return 0; // No digits after decimal

    // Extract full number
    char full_num[128];
    int j = 0;
    for (int k = pos; k < i; k++) {
        full_num[j++] = str[k];
    }
    full_num[j] = '\0';

    // Remove commas
    char clean_num[128];
    j = 0;
    for (size_t k = 0; k < strlen(full_num); k++) {
        if (full_num[k] != ',')
            clean_num[j++] = full_num[k];
    }
    clean_num[j] = '\0';

    // Split into integer and fractional parts
    char* dot = strchr(clean_num, '.');
    if (! dot)
        return 0;

    *dot                = '\0';
    int integer_part    = atoi(clean_num);
    int fractional_part = atoi(dot + 1);

    char int_words[128], frac_words[128];
    number_to_words(integer_part, int_words, sizeof(int_words));
    number_to_words(fractional_part, frac_words, sizeof(frac_words));

    snprintf(out, size, "%s point %s", int_words, frac_words);
    return i - pos;
}

static int parse_number(const char* str, int pos, char* out, size_t size)
{
    if (! isdigit(str[pos]))
        return 0;

    int i           = pos;
    int digit_count = 0;

    // Allow digits and commas (only between groups of 3 digits)
    while (1) {
        if (isdigit(str[i])) {
            digit_count++;
            i++;
        } else if (str[i] == ',' && digit_count > 0 && isdigit(str[i + 1])) {
            // Only allow comma if followed by more digits
            i++;
            digit_count = 0; // Reset for next group
        } else {
            break;
        }
    }

    if (i == pos)
        return 0; // No number found

    // Copy number and remove commas
    char num_str[128];
    int j = 0;
    for (int k = pos; k < i; k++) {
        if (str[k] != ',')
            num_str[j++] = str[k];
    }
    num_str[j] = '\0';

    int num = atoi(num_str);
    number_to_words(num, out, size);

    return i - pos;
}

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} Buffer;

static void buffer_init(Buffer* buf)
{
    buf->data    = (char*)malloc(128);
    buf->cap     = 128;
    buf->len     = 0;
    buf->data[0] = '\0';
}

static void buffer_append(Buffer* buf, const char* str)
{
    size_t needed = buf->len + strlen(str) + 1;
    if (needed > buf->cap) {
        while (needed > buf->cap)
            buf->cap *= 2;
        buf->data = (char*)realloc(buf->data, buf->cap);
    }
    strcpy(buf->data + buf->len, str);
    buf->len += strlen(str);
}

static void buffer_free(Buffer* buf) { free(buf->data); }

char* normalize_numbers(const char* text)
{
    Buffer out;
    buffer_init(&out);

    int i          = 0;
    char temp[288] = {0};

    while (text[i]) {
        int matched = 0;

        if ((matched = parse_ordinal(text, i, temp, sizeof(temp)))) {
            buffer_append(&out, temp);
            i += matched;
        } else if ((matched = parse_dollar(text, i, temp, sizeof(temp)))) {
            buffer_append(&out, temp);
            i += matched;
        } else if ((matched = parse_decimal(text, i, temp, sizeof(temp)))) {
            buffer_append(&out, temp);
            i += matched;
        } else if ((matched = parse_number(text, i, temp, sizeof(temp)))) {
            buffer_append(&out, temp);
            i += matched;
        } else {
            char c[2] = {text[i++], '\0'};
            buffer_append(&out, c);
        }
    }

    char* result = (char*)calloc(out.len + 1, 1);
    strncpy(result, out.data, out.len);
    buffer_free(&out);
    return result;
}

static int is_sentence_end(char c) { return (c == '.' || c == '!' || c == '?' || c == '\n'); }

static const char* skip_leading_whitespace(const char* str)
{
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

void split_by_sentences(const char* text, sentence_callback_t callback, void* user_data)
{
    if (! text || ! callback) {
        return;
    }

    const char* start  = text;
    const char* ptr    = start;
    int in_quote       = 0;
    int sentence_found = 0;

    while (*ptr) {
        sentence_found           = 0;
        const char* sentence_end = NULL;

        if (*ptr == '"') {
            // If opening quote and there's pending text before it, split it
            if (! in_quote && ptr > start) {
                callback(start, ptr - start, user_data);
                start = ptr;
            }

            // Toggle quote status
            in_quote = ! in_quote;

            // If closing quote and the previous character is a sentence end
            if (! in_quote && ptr > start && is_sentence_end(*(ptr - 1))) {
                sentence_end   = ptr + 1;
                sentence_found = 1;
            }
        } else if (! in_quote && is_sentence_end(*ptr)) {
            // Sentence ends at this punctuation
            sentence_end   = ptr + 1;
            sentence_found = 1;
        }

        // If a sentence boundary is found
        if (sentence_found && sentence_end) {
            const char* next_actual_sentence_start = skip_leading_whitespace(sentence_end);

            // Correctly calculate length up to the end of the sentence
            size_t len = sentence_end - start;
            if (len > 0) {
                callback(start, len, user_data);
            }

            // Move start to the next non-whitespace character
            start = next_actual_sentence_start;
            ptr   = next_actual_sentence_start;

            // If we've reached the end of the string, break
            if (! *ptr) {
                break;
            }

            continue; // Skip the default ptr increment
        }

        ptr++; // Move to the next character
    }

    // Handle any remaining text after the last sentence
    if (start < ptr) {
        callback(start, ptr - start, user_data);
    }
}
