#include "board.h"
#include "hx_drv_iic.h"
#include "hx_drv_scu.h"
#include "hx_drv_scu_export.h"
#include "hx_drv_timer.h"

#include "crc_table.h"
#include "i2c_protocol.h"
#include "i2cs.h"

#include "stdio.h"

#define I2C_DEV_ID         USE_DW_IIC_SLV_1
#define I2C_SLAVE_ADDR     0x24
#define I2C_TX_TIME_OUT_MS 2000

static HX_DRV_DEV_IIC* dev_iic_slv;

static int i2cs_initialized = 0;

static int i2c_rx_done   = 1;
static int i2c_tx_done   = 1;
static uint32_t rx_bytes = 0;
static uint32_t tx_bytes = 0;

void i2cs_tx_timeout_cb(void* data)
{
    // if i2c slave tx complete timeout, reset i2c slave
    printf("\n\n#### [%s], i2cs_reset ####\n\n", __FUNCTION__);
    hx_drv_i2cs_reset(I2C_DEV_ID);
    i2cs_deinit();
    i2cs_init();
}

void i2cs_tx_timer(uint32_t timeout_ms)
{
    TIMER_CFG_T timer_cfg;
    timer_cfg.period = timeout_ms;
    timer_cfg.mode   = TIMER_MODE_ONESHOT;
    timer_cfg.ctrl   = TIMER_CTRL_CPU;
    timer_cfg.state  = TIMER_STATE_DC;

    hx_drv_timer_cm55s_stop();
    hx_drv_timer_cm55s_start(&timer_cfg, (Timer_ISREvent_t)i2cs_tx_timeout_cb);
}

static void i2cs_err_cb(void* param)
{
    HX_DRV_DEV_IIC* iic_obj           = (HX_DRV_DEV_IIC*)param;
    HX_DRV_DEV_IIC_INFO* iic_info_ptr = &(iic_obj->iic_info);
    uint32_t err_state                = iic_info_ptr->err_state;
    // printf("[%s] err:%lu \n", __FUNCTION__, err_state);

    // if (iic_info_ptr->err_state == DEV_IIC_ERR_TX_DATA_UNREADY) {
    //     memcpy(tx_buffer, &tx_packet, sizeof(packet_header_t));
    //     i2c_write_en(sizeof(packet_header_t));
    // }
}

static void i2cs_rx_cb(void* param)
{
    HX_DRV_DEV_IIC* iic_obj           = (HX_DRV_DEV_IIC*)param;
    HX_DRV_DEV_IIC_INFO* iic_info_ptr = &(iic_obj->iic_info);
    uint32_t len, ofs;
    uint8_t* rbuffer;

    len     = iic_info_ptr->rx_buf.len;
    ofs     = iic_info_ptr->rx_buf.ofs;
    rbuffer = (uint8_t*)iic_info_ptr->rx_buf.buf;

    // printf("[%s] recv %lu/%lu bytes\n", __FUNCTION__, ofs, len);

    rx_bytes    = ofs;
    i2c_rx_done = 1;
}

static void i2cs_tx_cb(void* param)
{
    hx_drv_timer_cm55s_stop();

    HX_DRV_DEV_IIC* iic_obj           = (HX_DRV_DEV_IIC*)param;
    HX_DRV_DEV_IIC_INFO* iic_info_ptr = &(iic_obj->iic_info);

    uint32_t len = iic_info_ptr->tx_buf.len;
    uint32_t ofs = iic_info_ptr->tx_buf.ofs;

    // printf("[%s] sent %lu/%lu bytes\n", __FUNCTION__, ofs, len);

    tx_bytes    = ofs;
    i2c_tx_done = 1;
}

void i2cs_init()
{
    printf("[%s]\n", __FUNCTION__);
    hx_drv_scu_set_PB5_pinmux(SCU_PB5_PINMUX_I2C_S1_SCL, 1);
    hx_drv_scu_set_PB6_pinmux(SCU_PB6_PINMUX_I2C_S1_SDA, 1);

    hx_drv_i2cs_init(I2C_DEV_ID, HX_I2C_HOST_SLV_1_BASE);

    dev_iic_slv = hx_drv_i2cs_get_dev(I2C_DEV_ID);
    dev_iic_slv->iic_control(DW_IIC_CMD_SET_TXCB, (void*)i2cs_tx_cb);
    dev_iic_slv->iic_control(DW_IIC_CMD_SET_RXCB, (void*)i2cs_rx_cb);
    dev_iic_slv->iic_control(DW_IIC_CMD_SET_ERRCB, (void*)i2cs_err_cb);
    dev_iic_slv->iic_control(DW_IIC_CMD_SLV_SET_SLV_ADDR, (void*)I2C_SLAVE_ADDR);

    rx_bytes    = 0;
    tx_bytes    = 0;
    i2c_rx_done = 1;
    i2c_tx_done = 1;

    i2cs_initialized = 1;
}

void i2cs_deinit()
{
    printf("[%s]\n", __FUNCTION__);
    hx_drv_i2cs_deinit(I2C_DEV_ID);

    i2cs_initialized = 0;
    dev_iic_slv = NULL;
}

void i2cs_read(void* buffer, size_t len)
{
    i2c_rx_done = 0;
    rx_bytes    = 0;
    hx_drv_i2cs_interrupt_read(I2C_DEV_ID, I2C_SLAVE_ADDR, buffer, len, (void*)i2cs_rx_cb);
}

void i2cs_write(void* buffer, size_t len)
{
    i2c_tx_done = 0;
    tx_bytes    = 0;
    hx_drv_i2cs_interrupt_write(I2C_DEV_ID, I2C_SLAVE_ADDR, buffer, len, (void*)i2cs_tx_cb);
    // register a tx timeout callback to avoid i2c slave halted
    i2cs_tx_timer(I2C_TX_TIME_OUT_MS);
}

uint32_t i2cs_wait_rx_done()
{
    while (! i2c_rx_done) {
        board_delay_ms(1);
    }
    return rx_bytes;
}

uint32_t i2cs_wait_tx_done()
{
    while (! i2c_tx_done) {
        board_delay_ms(1);
    }
    return tx_bytes;
}

int i2c_send_packet(uint8_t status, uint8_t error, uint8_t type, uint16_t packet_index, uint16_t total_packets,
                    const uint8_t* payload, uint16_t payload_length)
{
    if (payload_length > PAYLOAD_SIZE) {
        return -1; // overflow
    }

    static hm_tx_packet_t tx_packet;
    tx_packet.status                = status;
    tx_packet.error                 = error;
    tx_packet.payload.type          = type;
    tx_packet.payload.packet_index  = packet_index;
    tx_packet.payload.total_packets = total_packets;
    tx_packet.payload.data_length   = payload_length;

    memset(tx_packet.payload.data, 0, PAYLOAD_SIZE);

    // Copy payload
    memcpy(tx_packet.payload.data, payload, payload_length);

    tx_packet.crc16 =
        crc16_compute(crc16_lut, &crc16_ccitt_false_config, (const uint8_t*)&tx_packet.payload, sizeof(payload_t));

    i2cs_write(&tx_packet, sizeof(tx_packet));
    return 0;
}

int i2c_recv_string(uint8_t* buffer, size_t len)
{
    static hm_rx_packet_t rx_packet;
    memset(&rx_packet, 0, sizeof(hm_rx_packet_t));
    uint16_t buffer_idx = 0;
    do {
        i2cs_read(&rx_packet, sizeof(hm_rx_packet_t));
        if (i2cs_wait_rx_done() == 0) {
            return 0;
        }
        for (size_t i = 0; i < rx_packet.payload.data_length; i++) {
            buffer[buffer_idx++ % len] = rx_packet.payload.data[i];
        }
        uint16_t crc16 =
            crc16_compute(crc16_lut, &crc16_ccitt_false_config, (const uint8_t*)&rx_packet.payload, sizeof(payload_t));
        // printf("packet %u/%u, %u bytes\n", rx_packet.payload.packet_index, rx_packet.payload.total_packets,
        //        rx_packet.payload.data_length);
        // printf("crc16: %#x, %#x\n", rx_packet.crc16, crc16);
        // printf("buffer_idx=%u\n", buffer_idx);
    } while ((rx_packet.payload.packet_index + 1) < rx_packet.payload.total_packets);
    buffer[buffer_idx % len] = '\0';

    if (buffer_idx > len) {
        printf("buffer overflow: %u\n", buffer_idx);
        return buffer_idx - len;
    } else {
        return buffer_idx;
    }
}

int i2c_send_string(const uint8_t* buffer, size_t len)
{
    uint16_t tx_chunk_num       = len / PAYLOAD_SIZE;
    uint16_t tx_rem_data        = len % PAYLOAD_SIZE;
    uint16_t tx_total_chunk_num = tx_rem_data ? tx_chunk_num + 1 : tx_chunk_num;

    for (size_t chunk_idx = 0; chunk_idx < tx_chunk_num; chunk_idx++) {
        const uint8_t* data = (const uint8_t*)&buffer[chunk_idx * PAYLOAD_SIZE];
        i2c_send_packet(ST_LLM_OUTPUT_RDY, ERR_NONE, TYPE_TEXT_ASCII, chunk_idx, tx_total_chunk_num, data,
                        PAYLOAD_SIZE);
        if (i2cs_wait_tx_done() == 0) {
            printf("text tx timeout\n");
            return -1;
        }
    }
    if (tx_rem_data) {
        const uint8_t* data = (const uint8_t*)&buffer[len] - tx_rem_data;
        i2c_send_packet(ST_LLM_OUTPUT_RDY, ERR_NONE, TYPE_TEXT_ASCII, tx_chunk_num, tx_total_chunk_num, data,
                        tx_rem_data);
        if (i2cs_wait_tx_done() == 0) {
            printf("text tx timeout\n");
            return -1;
        }
    }
    i2c_send_packet(ST_EOT, ERR_NONE, TYPE_NONE, 0, 0, 0, 0);
    if (i2cs_wait_tx_done() == 0) {
        printf("text tx timeout\n");
        return -1;
    }

    return 0;
}

int i2c_send_audio(int16_t* audio_buffer, size_t samples_num, size_t frame_idx, size_t frame_num)
{
    uint16_t tx_chunk_num       = samples_num / PAYLOAD_AUDIO_SAMPLES;
    uint16_t tx_rem_data        = samples_num % PAYLOAD_AUDIO_SAMPLES * sizeof(int16_t);
    uint16_t tx_total_chunk_num = tx_rem_data ? tx_chunk_num + 1 : tx_chunk_num;
    // printf("tx_total_chunk_num=%u, tx_rem=%u\n", tx_total_chunk_num, tx_rem_data);

    uint32_t t1, t2;
    t1 = get_timestamp_ms();
    for (size_t chunk_idx = 0; chunk_idx < tx_chunk_num; chunk_idx++) {
        const uint8_t* data = (const uint8_t*)&audio_buffer[chunk_idx * PAYLOAD_AUDIO_SAMPLES];
        i2c_send_packet(ST_TTS_OUTPUT_RDY, ERR_NONE, TYPE_AUDIO_PCM16, frame_idx * tx_total_chunk_num + chunk_idx,
                        tx_total_chunk_num * frame_num, data, PAYLOAD_SIZE);
        if (i2cs_wait_tx_done() == 0) {
            printf("audio tx timeout\n");
            return -1;
        }
    }
    if (tx_rem_data) {
        const uint8_t* data = (const uint8_t*)&audio_buffer[samples_num] - tx_rem_data;
        i2c_send_packet(ST_TTS_OUTPUT_RDY, ERR_NONE, TYPE_AUDIO_PCM16, frame_idx * tx_total_chunk_num + tx_chunk_num,
                        tx_total_chunk_num * frame_num, data, tx_rem_data);
        if (i2cs_wait_tx_done() == 0) {
            printf("audio tx timeout\n");
            return -1;
        }
    }
    t2 = get_timestamp_ms();
    printf("audio tx: %ld ms\n", t2 - t1);

    return 0;
}

int i2cs_is_initialized(void)
{
    return i2cs_initialized && (dev_iic_slv != NULL);
}
