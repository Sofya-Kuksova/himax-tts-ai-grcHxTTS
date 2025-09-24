#ifndef I2C_PROTOCOL_H_
#define I2C_PROTOCOL_H_

#include <stdint.h>

// Status byte values
enum {
    ST_IDLE           = 0x00,
    ST_LLM_OUTPUT_RDY = 0x01,
    ST_TTS_OUTPUT_RDY = 0x02,
    ST_EOT            = 0x03,
};

// Error codes
enum {
    ERR_NONE = 0x00,
};

// Data‑type byte values
enum {
    TYPE_NONE        = 0x00,
    TYPE_TEXT_ASCII  = 0x01,
    TYPE_AUDIO_PCM16 = 0x02,
};

#define PAYLOAD_SIZE          512
#define PAYLOAD_AUDIO_SAMPLES (PAYLOAD_SIZE / sizeof(int16_t))

#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint16_t packet_index;
    uint16_t total_packets;
    uint16_t data_length;
    uint8_t data[PAYLOAD_SIZE];
} payload_t;

typedef struct {
    payload_t payload;
    uint16_t crc16;
} hm_rx_packet_t;

typedef struct {
    uint8_t status;
    uint8_t error;
    payload_t payload;
    uint16_t crc16;
} hm_tx_packet_t;
#pragma pack(pop)

#endif // I2C_PROTOCOL_H_
