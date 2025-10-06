#ifndef I2C_H_
#define I2C_H_

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void i2cs_init();
void i2cs_deinit();
void i2cs_read(void* buffer, size_t len);
void i2cs_write(void* buffer, size_t len);
uint32_t i2cs_wait_rx_done();
uint32_t i2cs_wait_tx_done();

int i2c_send_packet(uint8_t status, uint8_t error, uint8_t type, uint16_t packet_index, uint16_t total_packets,
                    const uint8_t* payload, uint16_t payload_length);
int i2c_send_string(const uint8_t* buffer, size_t len);
int i2c_recv_string(uint8_t* buffer, size_t len);
int i2c_send_audio(int16_t* audio_buffer, size_t samples_num, size_t frame_idx, size_t frame_num);

int i2cs_is_initialized(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // I2C_H_
