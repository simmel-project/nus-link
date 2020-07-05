/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// #define RECORD_TEST_16
// #define RECORD_TEST_32

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nrf_clock.h"
#include "nrfx.h"
#include "nrfx_config.h"
#include "nrfx_power.h"
#include "nrfx_pwm.h"

#include "nrf.h"

#include "nrfx_nvmc.h"
#include "nrfx_spi.h"

#include "bpsk.h"
#include "i2s.h"
#include "spi.h"
#include "usb.h"
#include "modulate.h"

#include "printf.h"

#define SAMPLE_RATE 62500
#define CARRIER_TONE 20833.33f
#define BAUD_RATE (651.0417f) // 31.25
#define PLL_INCR (BAUD_RATE / (float)(SAMPLE_RATE))

#define TEST_STRING                                                            \
    "Article 1. "\
    "All human beings are born free and equal in dignity and rights. "         \
    "They are endowed with reason and conscience and should act towards one "  \
    "another in a spirit of brotherhood. "                                     \
    "Article 2. "\
    "Everyone is entitled to all the rights and freedoms set forth in this "   \
    "Declaration, without distinction of any kind, such as race, colour, sex, "\
    "language, religion, political or other opinion, national or social "      \
    "origin, property, birth or other status. Furthermore, no distinction "    \
    "shall be made on the basis of the political, jurisdictional or "          \
    "international status of the country or territory to which a person "      \
    "belongs, whether it be independent, trust, non-self-governing or under "  \
    "any other limitation of sovereignty. " \
    "Article 3. "\
    "Everyone has the right to life, liberty and security of person. "\
    "##### "\

#define TEST_STRING2                                                            \
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"       \
    "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT"      \
    "3333333333333333333333333333333333333333333333333333333333333333333"

struct modulate_state mod_instance;

#define BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS 0x0007E000
// This must match the value in `nrf52833_s140_v7.ld`
#define BOOTLOADER_REGION_START                                                \
    0x00070000 /**< This field should correspond to start address of the       \
                  bootloader, found in UICR.RESERVED, 0x10001014, register.    \
                  This value is used for sanity check, so the bootloader will  \
                  fail immediately if this value differs from runtime value.   \
                  The value is used to determine max application size for      \
                  updating. */
#define CODE_PAGE_SIZE                                                         \
    0x1000 /**< Size of a flash codepage. Used for size of the reserved flash  \
              space in the bootloader region. Will be runtime checked against  \
              NRF_UICR->CODEPAGESIZE to ensure the region is correct. */
uint8_t m_mbr_params_page[CODE_PAGE_SIZE] __attribute__((section(
    ".mbrParamsPage"))); /**< This variable reserves a codepage for mbr
                            parameters, to ensure the compiler doesn't locate
                            any code or variables at his location. */
volatile uint32_t m_uicr_mbr_params_page_address
    __attribute__((section(".uicrMbrParamsPageAddress"))) =
        BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS;

uint8_t m_boot_settings[CODE_PAGE_SIZE] __attribute__((
    section(".bootloaderSettings"))); /**< This variable reserves a codepage for
                                         bootloader specific settings, to ensure
                                         the compiler doesn't locate any code or
                                         variables at his location. */
volatile uint32_t m_uicr_bootloader_start_address __attribute__((
    section(".uicrBootStartAddress"))) =
    BOOTLOADER_REGION_START; /**< This variable ensures that the linker script
                                will write the bootloader start address to the
                                UICR register. This value will be written in the
                                HEX file and thus written to UICR when the
                                bootloader is flashed into the chip. */

__attribute__((used)) uint8_t id[3];
#if defined(RECORD_TEST_16) || defined(RECORD_TEST_32)
#define RECORD_BUFFER_SIZE 1024
#else
#define RECORD_BUFFER_SIZE 4096
#endif
__attribute__((used, aligned(4))) int8_t record_buffer[RECORD_BUFFER_SIZE];

static const struct i2s_pin_config i2s_config = {
    .data_pin_number = (32 + 9),
    .bit_clock_pin_number = (0 + 12),
    .word_select_pin_number = (0 + 8),
};

volatile uint32_t i2s_irqs;
volatile int16_t *i2s_buffer = NULL;
volatile bool i2s_ready = false;

#ifdef RECORD_TEST_32
static uint8_t data_buffer[81920];
static uint32_t data_buffer_offset;
#endif

#ifdef RECORD_TEST_16
#define DATA_BUFFER_ELEMENT_COUNT (81920 / 2)
static int16_t data_buffer[DATA_BUFFER_ELEMENT_COUNT];
static uint32_t data_buffer_offset;
#endif

volatile float agc_offset = 100000;
volatile float agc_increase = 1.01;
volatile float agc_decrease = 0.99995;
volatile float agc_target = 50000.0;
volatile float agc_min = 10;
volatile float agc_reduce_threshold = 300;
uint32_t clipped_samples;

extern uint32_t samplecount;
static void background_tasks(void) {
    tud_task(); // tinyusb device task
    cdc_task();
}

int main(void) {
    int i;

    NRF_POWER->DCDCEN = 1UL;

    // Switch to the lfclk
    NRF_CLOCK->TASKS_LFCLKSTOP = 1UL;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->TASKS_LFCLKSTART = 1UL;

    // turn on HFCLK for accurate modulation
    NRF_CLOCK->TASKS_HFCLKSTART = 1UL;

    usb_init();
    tusb_init();
    bpsk_init();

    spi_init();

    // Get the SPI ID, just to make sure things are working.
    spi_select();
    spi_xfer(0x9f);
    id[0] = spi_xfer(0xff);
    id[1] = spi_xfer(0xff);
    id[2] = spi_xfer(0xff);
    spi_deselect();

    // Issue "Deep Power Down"
    spi_select();
    spi_xfer(0xb9);
    spi_deselect();

    spi_deinit();

    uint32_t rate = SAMPLE_RATE;
    uint32_t tone = CARRIER_TONE; // rate / 3;
    modulate_init(&mod_instance, tone, rate, TEST_STRING);

    nus_init(&i2s_config, record_buffer, sizeof(record_buffer));
    nus_start();

    while (!tud_cdc_n_connected(0)) {
        background_tasks();
    }

    // USB CDC is always unreliable during first connection. Use this cheesy
    // delay to work around that problem.
    for (i = 0; i < 16384; i++) {
        background_tasks();
    }

    printf("SPI ID: %02x %02x %02x\n", id[0], id[1], id[2]);
    printf("Hello world!\n");
    printf("pwm0 inten: %x\n", NRF_PWM0->INTEN);
    printf("pwm0 periodend: %x\n", NRF_PWM0->EVENTS_PWMPERIODEND);

    uint32_t usb_irqs = 0;
    uint32_t last_i2s_irqs = i2s_irqs;
    // uint32_t last_i2s_runs = 0;
    while (1) {
        background_tasks();
        usb_irqs++;
        if (i2s_irqs != last_i2s_irqs) {
            // printf("I2S IRQ count: %d  USB IRQ count: %d\n", i2s_irqs,
            // usb_irqs);
            last_i2s_irqs = i2s_irqs;
            usb_irqs = 0;
        }
        // if (((i2s_runs & 127) == 0) && (i2s_runs != last_i2s_runs)) {
        //     // printf("div: %d  tar: %5.0f  red: %f  ", agc_div, agc_target,
        //     //        agc_decrease);
        //     // tud_cdc_n_write_flush(0);
        //     // printf("inc: %f  off: %f\n", agc_increase, agc_offset);
        //     // last_i2s_runs = i2s_runs;
        //     // tud_cdc_n_write_flush(0);
        //     printf("div: %d  tar: %5.0f  dec: %f  "
        //            "inc: %f  off: %f  clip: %d  peak: %d  real: %d\n",
        //            _div, agc_target, agc_decrease, agc_increase,
        //            agc_offset, clipped_samples, current_peak, current_peak /
        //            agc_div);
        //     last_i2s_runs = i2s_runs;
        //     clipped_samples = 0;
        //     tud_cdc_n_write_flush(0);
        // }
    }

    NVIC_SystemReset();
}

// printf glue
void _putchar(char character) {
    int buffered = 1;
    if (character == '\n') {
        tud_cdc_n_write_char(0, '\r');
    }
    if (buffered) {
        while (tud_cdc_n_write_char(0, character) < 1) {
            tud_cdc_n_write_flush(0);
            background_tasks();
        }
    } else {
        tud_cdc_n_write_char(0, character);
    }
}
