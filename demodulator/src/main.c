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
#include "nrf_timer.h"

#include "nrfx_nvmc.h"
#include "nrfx_spi.h"

#include "bpsk.h"
#include "i2s.h"
#include "spi.h"
#include "usb.h"

#include "printf.h"

// This exposes a struct called `run_time` that can be used
// to determine how long it takes to do various tasks.
#define MEASURE_RUNTIME

// Enable one of these in order to record to a RAM buffer
// rather than sending audio data to the demodulator.

//#define RECORD_TEST_16
//#define RECORD_TEST_32

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
#define DATA_BUFFER_ELEMENT_COUNT (81920 / 4)
static uint8_t data_buffer[DATA_BUFFER_ELEMENT_COUNT];
#endif

#ifdef RECORD_TEST_16
#define DATA_BUFFER_ELEMENT_COUNT (81920 / 2)
static int16_t data_buffer[DATA_BUFFER_ELEMENT_COUNT];
#endif

uint32_t i2s_runs = 0;

#ifdef MEASURE_RUNTIME
#define TIMER_OBJ (NRF_TIMER0)
uint32_t run_processing_start;
uint32_t run_idle_start;
struct run_time {
    uint32_t capture;
    uint32_t idle;
};

// You can figure out how much time is spent doing work by running:
// (gdb) p run_time.capture * 100.0 / (run_time.capture  + run_time.idle)
// $1 = 64.517458185813695
// (gdb)
// In the above example, 64% of time is spent demoduating.
struct run_time run_time;

static void timer_init(void) {
    nrf_timer_mode_set(TIMER_OBJ, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(TIMER_OBJ, NRF_TIMER_BIT_WIDTH_32);
    nrf_timer_frequency_set(TIMER_OBJ, NRF_TIMER_FREQ_16MHz);
    nrf_timer_task_trigger(TIMER_OBJ, NRF_TIMER_TASK_START);
}

static uint32_t timer_capture(void) {
    const uint32_t channel_number = 0;
    nrf_timer_task_trigger(TIMER_OBJ,
                           nrf_timer_capture_task_get(channel_number));
    return nrf_timer_cc_get(TIMER_OBJ, channel_number);
}

static void runtime_work_start(void) {
    run_time.capture = run_idle_start - run_processing_start;
    run_processing_start = timer_capture();
    run_time.idle = run_processing_start - run_idle_start;
}

static void runtime_idle_start(void) {
    run_idle_start = timer_capture();
}
#else
static void timer_init(void) {
}
static void runtime_work_start(void) {
}
static void runtime_idle_start(void) {
}
#endif

static void background_tasks(void) {
    tud_task(); // tinyusb device task
    cdc_task();

    if (i2s_ready) {

        i2s_ready = false;
        i2s_runs++;

        // Indicate to the timer system that we've started work
        runtime_work_start();

        const size_t samples_per_loop =
            RECORD_BUFFER_SIZE / 2 / sizeof(uint32_t) / 2;

        // Cast to 32-bit int pointer, since we need to swap the values.
        uint32_t *input_buffer = (uint32_t *)i2s_buffer;
        unsigned int i;

#if defined(RECORD_TEST_16)
        static int16_t *output_buffer_ptr = &data_buffer[0];
        // Every other 32-bit word in `input_buffer` is 0, so skip over it.
        // Additionally, we're only interested in one half of the record
        // buffer, since the other half is being written to.
        for (i = 0; i < samples_per_loop; i++) {
            uint32_t unswapped = input_buffer[i * 2];
            int32_t swapped = (int32_t)((((unswapped >> 16) & 0xffff) |
                                         ((unswapped << 16) & 0xffff0000)));
            *output_buffer_ptr++ = swapped;

            // Advance the output buffer pointer, wrapping when it reaches
            // the end.
            if (output_buffer_ptr >= &data_buffer[DATA_BUFFER_ELEMENT_COUNT])
                output_buffer_ptr = &data_buffer[0];
        }
#elif defined(RECORD_TEST_32)
        // Used for sampling during development. Read out the buffer with:
        // ```
        // (gdb) dump binary memory tone.raw data_buffer
        // data_buffer+sizeof(data_buffer)
        // ```
        static int32_t *output_buffer_ptr = (int32_t *)&data_buffer[0];
        for (i = 0; i < samples_per_loop; i++) {
            uint32_t unswapped = input_buffer[i * 2];
            int32_t swapped = (int32_t)((((unswapped >> 16) & 0xffff) |
                                         ((unswapped << 16) & 0xffff0000)));
            *output_buffer_ptr++ = swapped;
            if (output_buffer_ptr >=
                (int32_t *)&data_buffer[DATA_BUFFER_ELEMENT_COUNT])
                output_buffer_ptr = (int32_t *)&data_buffer[0];
        }
#else
        // Every other 32-bit word in `input_buffer` is 0, so skip over it.
        // Additionally, we're only interested in one half of the record
        // buffer, since the other half is being written to.
        int32_t output_buffer[samples_per_loop];
        int32_t *output_buffer_ptr = output_buffer;
        for (i = 0; i < samples_per_loop; i++) {
            uint32_t unswapped = input_buffer[i * 2];
            int32_t swapped = (int32_t)((((unswapped >> 16) & 0xffff) |
                                         ((unswapped << 16) & 0xffff0000)));
            *output_buffer_ptr++ = swapped;
        }
        bpsk_run(output_buffer, sizeof(output_buffer) / sizeof(*output_buffer));
#endif

        // Indicate to the measurement system that work is done and we're
        // idle now.
        runtime_idle_start();
        if (i2s_ready) {
	  printf("I2S UNDERRUN!!!\n");
        }
    }
}

int main(void) {
    int i;

    NRF_POWER->DCDCEN = 1UL;

    // Switch to the lfclk
    NRF_CLOCK->TASKS_LFCLKSTOP = 1UL;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->TASKS_LFCLKSTART = 1UL;

    // run HFCLK for better demodulation precision, at higher power consumption
    NRF_CLOCK->TASKS_HFCLKSTART = 1UL;

    usb_init();
    tusb_init();
    bpsk_init();

    spi_init();

    timer_init();

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

    uint32_t usb_irqs = 0;
    uint32_t last_i2s_irqs = i2s_irqs;
    while (1) {
        background_tasks();
        usb_irqs++;
        if (i2s_irqs != last_i2s_irqs) {
            last_i2s_irqs = i2s_irqs;
            usb_irqs = 0;
        }
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
