#ifndef UTILS_H_
#define UTILS_H_

#include "stdint.h"
#include "stdio.h"

void printBase64(const uint8_t* data, size_t len)
{
    static const char* BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t count              = len / 3;
    size_t remainder          = len % 3;
    char buf[]                = "====";
    printf("====");
    while (count--) {
        buf[0] = BASE64[data[0] >> 2];
        buf[1] = BASE64[(data[0] & 3) << 4 | data[1] >> 4];
        buf[2] = BASE64[(data[1] & 0xf) << 2 | data[2] >> 6];
        buf[3] = BASE64[data[2] & 0x3f];
        printf("%s", buf);
        data += 3;
    }
    if (remainder) {
        uint8_t b2 = remainder > 1 ? data[1] : 0;
        buf[0]     = BASE64[data[0] >> 2];
        buf[1]     = BASE64[(data[0] & 3) << 4 | b2 >> 4];
        buf[2]     = remainder > 1 ? BASE64[(b2 & 0xf) << 2] : '=';
        buf[3]     = '=';
        printf("%s", buf);
    }
    printf("\n");
}

static void hexdump(const uint8_t* buffer, size_t len)
{
#define CHARS_PER_LINE 16
    for (size_t i = 0; i < len / CHARS_PER_LINE; i++) {
        printf("0x%p\t", &buffer[i * CHARS_PER_LINE]);
        for (size_t j = 0; j < CHARS_PER_LINE; j++) {
            printf("%02x ", buffer[i * CHARS_PER_LINE + j]);
        }
        printf("\t| ");
        for (size_t j = 0; j < CHARS_PER_LINE; j++) {
            char c = buffer[i * CHARS_PER_LINE + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf(" |\n");
    }
    size_t rem = len % CHARS_PER_LINE;
    if (rem) {
        printf("0x%p\t", &buffer[len - rem]);

        for (size_t j = 0; j < rem; j++) {
            printf("%02x ", buffer[len - rem + j]);
        }
        for (size_t j = 0; j < CHARS_PER_LINE - rem; j++) {
            printf("00 ");
        }
        printf("\t| ");
        for (size_t j = 0; j < rem; j++) {
            char c = buffer[len - rem + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        for (size_t j = 0; j < CHARS_PER_LINE - rem; j++) {
            char c = 0;
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf(" |\n");
    }
}

#endif // UTILS_H_
