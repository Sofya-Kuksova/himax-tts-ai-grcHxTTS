#ifndef UART_H_
#define UART_H_

struct uart_msg_header_t {
    uint8_t magick[2]; // 0x1, 0xe
    uint16_t message_length;
};

void uart_init();
void uart_deinit();
void uart_clear();
int uart_read(char* buffer, size_t size);

#endif // UART_H_
