#include "board.h"
#include "hx_drv_scu.h"
#include "hx_drv_scu_export.h"
#include "hx_drv_timer.h"
#include "hx_drv_uart.h"

#include "uart.h"

#define MIN(A, B) ((A) < (B) ? (A) : (B))

static DEV_UART_PTR dev_uart_ptr;

void uart_init()
{
    hx_drv_scu_set_PB0_pinmux(SCU_PB0_PINMUX_UART0_RX_1, 1);
    hx_drv_scu_set_PB1_pinmux(SCU_PB1_PINMUX_UART0_TX_1, 1);
    hx_drv_uart_init(USE_DW_UART_0, HX_UART0_BASE);
    dev_uart_ptr = hx_drv_uart_get_dev(USE_DW_UART_0);
    dev_uart_ptr->uart_open(UART_BAUDRATE_921600);
}

void uart_deinit()
{
    dev_uart_ptr->uart_close();
    hx_drv_uart_init(USE_DW_UART_0, HX_UART0_BASE);
}

void uart_clear()
{
    int8_t buffer[8];
    while (dev_uart_ptr->uart_read_nonblock(buffer, sizeof(buffer))) {
    }
}

int uart_read(char* buffer, size_t size)
{
    uart_msg_header_t header = {0};
    dev_uart_ptr->uart_read(&header.magick[0], sizeof(header.magick[0]));
    if (header.magick[0] != 0x1) {
        uart_clear();
        return -1;
    }
    dev_uart_ptr->uart_read(&header.magick[1], sizeof(header.magick[1]));
    if (header.magick[1] != 0xE) {
        uart_clear();
        return -1;
    }
    printf("magick=0x");
    for (size_t i = 0; i < sizeof(header.magick); i++) {
        printf("%x", header.magick[i]);
    }
    dev_uart_ptr->uart_read(&header.message_length, sizeof(header.message_length));
    printf(", message_length=%u\n", header.message_length);
    dev_uart_ptr->uart_read(buffer, MIN(size, header.message_length));
    return 0;
}
