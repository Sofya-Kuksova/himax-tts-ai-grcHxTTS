#include "powermode_export.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WE2_CHIP_VERSION_C   0x8538000c
#define SPI_JPG_SEND_DIVIDER 10
#ifdef TRUSTZONE_SEC
#ifdef FREERTOS
/* Trustzone config. */
//
/* FreeRTOS includes. */
// #include "secure_port_macros.h"
#else
#if (__ARM_FEATURE_CMSE & 1) == 0
#error "Need ARMv8-M security extensions"
#elif (__ARM_FEATURE_CMSE & 2) == 0
#error "Compile with --cmse"
#endif
#include "arm_cmse.h"
// #include "veneer_table.h"
//
#endif
#endif

extern "C" {

#include "WE2_core.h"
#include "WE2_device.h"
#include "WE2_device_addr.h"
#include "hx_drv_gpio.h"
#include "hx_drv_scu.h"
#include "hx_drv_scu_export.h"
#include "hx_drv_spi.h"
#include "hx_drv_swreg_aon.h"
#include <hx_drv_pmu.h>
#include <hx_drv_pmu_export.h>

#include "BITOPS.h"
#include "powermode.h"

#include "ethosu_driver.h"
#include "spi_eeprom_comm.h"
}

#include "board.h"

#if defined(TRUSTZONE_SEC) || ! defined(TRUSTZONE)
#define U55_BASE BASE_ADDR_APB_U55_CTRL_ALIAS
#else
#define U55_BASE BASE_ADDR_APB_U55_CTRL
#endif

struct ethosu_driver g_ethosu_drv;

static void _arm_npu_irq_handler(void)
{
    /* Call the default interrupt handler from the NPU driver */
    ethosu_irq_handler(&g_ethosu_drv);
}

/**
 * @brief  Initialises the NPU IRQ
 **/
static void _arm_npu_irq_init(void)
{
    const IRQn_Type ethosu_irqnum = (IRQn_Type)U55_IRQn;

    /* Register the EthosU IRQ handler in our vector table.
     * Note, this handler comes from the EthosU driver */
    EPII_NVIC_SetVector(ethosu_irqnum, (uint32_t)_arm_npu_irq_handler);

    /* Enable the IRQ */
    NVIC_EnableIRQ(ethosu_irqnum);
}

static int _arm_npu_init(bool security_enable, bool privilege_enable)
{
    int err = 0;

    /* Initialise the IRQ */
    _arm_npu_irq_init();

    /* Initialise Ethos-U55 device */
    void* ethosu_base_address = (void*)(U55_BASE);

    if (0 != (err = ethosu_init(&g_ethosu_drv,        /* Ethos-U driver device pointer */
                                ethosu_base_address,  /* Ethos-U NPU's base address. */
                                NULL,                 /* Pointer to fast mem area - NULL for U55. */
                                0,                    /* Fast mem region size. */
                                security_enable,      /* Security enable. */
                                privilege_enable))) { /* Privilege enable. */
        printf("failed to initalise Ethos-U device\n");
        return err;
    }

    printf("Ethos-U55 device initialised\n");

    return 0;
}

#include "stories.h"
#include "time_calculation.h"

#include "i2cs.h"
#include "tts.hpp"

#define MAX_PROMPT_LEN 4096

static __attribute__((section(".arena"))) uint8_t arena[TOTAL_ARENA_SIZE] __ALIGNED(32);

int app_main(void)
{
    hx_lib_spi_eeprom_open(USE_DW_SPI_MST_Q);
    hx_lib_spi_eeprom_enable_XIP(USE_DW_SPI_MST_Q, true, FLASH_QUAD, true);

    if (_arm_npu_init(1, 1) != 0) {
        printf("Unable to init npu\n");
        return -1;
    }
#ifdef EXCHANGE_OVER_I2C
    i2cs_init();
#endif // EXCHANGE_OVER_I2C

    while (1) {
        TinyStoriesModel tinyStories;
        if (! tinyStories.init(MODEL_TINY_STORIES_BASE_ADDR, arena, TOTAL_ARENA_SIZE)) {
            printf("Unable to init TinyStories model\n");
            return -1;
        }

        printf("wait prompt\n");
        char prompt[MAX_PROMPT_LEN + 1] = {0};

#ifdef EXCHANGE_OVER_I2C
        if (i2c_recv_string((uint8_t*)prompt, MAX_PROMPT_LEN) == 0) {
            continue;
        }
        printf("recv prompt: %s\n", prompt);
#endif // EXCHANGE_OVER_I2C

        uint32_t started_at = get_timestamp_ms();
        int tokens          = 0;
        std::string story   = tinyStories.invoke(std::string(prompt), get_timestamp_ms(), [&](std::string token) {
            // print the token as it is ready
            tokens++;
            printf("%s", token.c_str());
            fflush(stdout);
#ifdef EXCHANGE_OVER_I2C
            i2c_send_string((uint8_t*)token.c_str(), token.size());
#endif // EXCHANGE_OVER_I2C
        });
        char summary_buffer[64];
        snprintf(summary_buffer, sizeof(summary_buffer), "\nTokens per second: %f\n\n",
                 tokens / ((get_timestamp_ms() - started_at) / 1000.0f));
#ifdef EXCHANGE_OVER_I2C
        i2c_send_string((const uint8_t*)summary_buffer, strlen(summary_buffer));
#endif // EXCHANGE_OVER_I2C
        printf(summary_buffer);

#ifdef ENABLE_TTS
        TTS tts;
        if (! tts.init(arena, TOTAL_ARENA_SIZE)) {
            printf("Unable to init TTS\n");
            return -1;
        }

        if (tts.run(story.c_str()) != 0) {
            printf("TTS failed\n");
            return -1;
        }
#endif // ENABLE_TTS

    }

    printf("====================================\n");

    return 0;
}