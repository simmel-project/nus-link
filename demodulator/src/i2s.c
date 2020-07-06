#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrfx.h"
#include "nrfx_i2s.h"
#include "nrfx_pwm.h"

#include "i2s.h"

static volatile uintptr_t buffers[2];
static volatile size_t buffer_size;
static volatile uint8_t buffer_num;

static uint16_t lrck_duty_cycles[PWM0_CH_NUM] = { 32 };

void nus_init(const struct i2s_pin_config *cfg, void *buffer, size_t len) {
    NRF_I2S->PSEL.MCK = (0 + 11); // Assign an unused pin
    //    NRF_I2S->PSEL.LRCK = cfg->word_select_pin_number;
    NRF_I2S->PSEL.LRCK = (0 + 24);
    NRF_I2S->PSEL.SCK = cfg->bit_clock_pin_number;
    NRF_I2S->PSEL.SDOUT = 0xFFFFFFFF;
    NRF_I2S->PSEL.SDIN = cfg->data_pin_number;

    NRF_I2S->CONFIG.MODE = I2S_CONFIG_MODE_MODE_Master;
    NRF_I2S->CONFIG.RXEN = I2S_CONFIG_RXEN_RXEN_Enabled;
    NRF_I2S->CONFIG.TXEN = I2S_CONFIG_TXEN_TXEN_Disabled;
    NRF_I2S->CONFIG.MCKEN = I2S_CONFIG_MCKEN_MCKEN_Enabled;
    NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_16Bit;
    NRF_I2S->CONFIG.RATIO = I2S_CONFIG_RATIO_RATIO_32X;
    NRF_I2S->CONFIG.MCKFREQ = I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV8;
    NRF_I2S->CONFIG.CHANNELS = I2S_CONFIG_CHANNELS_CHANNELS_Stereo;

    NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Left;
    NRF_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S;

    buffers[0] = (uintptr_t)buffer;
    buffers[1] = ((uintptr_t)buffer) + len / 2;
    buffer_size = len / 2;
    buffer_num = 0;

    NRF_PWM_Type* pwm    = NRF_PWM0;
    
    pwm->ENABLE = 0;

    pwm->PSEL.OUT[0] = cfg->word_select_pin_number;
    
    pwm->MODE            = PWM_MODE_UPDOWN_Up;
    pwm->COUNTERTOP      = 64;
    pwm->PRESCALER       = PWM_PRESCALER_PRESCALER_DIV_4; // 4 MHz, same as I2S
    pwm->DECODER         = PWM_DECODER_LOAD_Individual;
    pwm->LOOP            = 0;
  
    pwm->SEQ[0].PTR      = (uint32_t) (lrck_duty_cycles);
    pwm->SEQ[0].CNT      = 4; // default mode is Individual --> count must be 4
    pwm->SEQ[0].REFRESH  = 0;
    pwm->SEQ[0].ENDDELAY = 0;
    
    pwm->ENABLE = 1;
    
    pwm->EVENTS_SEQEND[0] = 0;
  
}

void nus_start(void) {

    // Load buffer number 0 into the pointer.
    buffer_num = 0;
    NRF_I2S->RXD.PTR = buffers[buffer_num & 1];
    NRF_I2S->TXD.PTR = 0xFFFFFFFF;
    NRF_I2S->RXTXD.MAXCNT = buffer_size / 4; // In units of 32-bit words

    NVIC_SetPriority(I2S_IRQn, 2);
    NVIC_EnableIRQ(I2S_IRQn);

    // Turn on the interrupt to the NVIC but not within the NVIC itself. This
    // will wake the CPU and keep it awake until it is serviced without
    // triggering an interrupt handler.
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
    NRF_I2S->INTENSET = I2S_INTENSET_RXPTRUPD_Msk;
    NRF_I2S->ENABLE = I2S_ENABLE_ENABLE_Enabled;

    NRF_I2S->TASKS_START = 1;
    return;
}

void nus_stop(void) {
    // Stop the task as soon as possible to prevent it from overwriting the
    // buffer.
    NVIC_DisableIRQ(I2S_IRQn);
    NRF_I2S->TASKS_STOP = 1;
    NRF_I2S->CONFIG.RXEN = I2S_CONFIG_RXEN_RXEN_Disabled;
    NRF_I2S->EVENTS_RXPTRUPD = 0;
    NRF_I2S->RXTXD.MAXCNT = 0;

    NRF_I2S->CONFIG.TXEN = I2S_CONFIG_TXEN_TXEN_Enabled;

    NRF_I2S->INTENCLR = I2S_INTENSET_RXPTRUPD_Msk;
    NRF_I2S->ENABLE = I2S_ENABLE_ENABLE_Disabled;
}

//--------------------------------------------------------------------+
// I2S has just picked up a buffer, so the previous buffer is ready.
//--------------------------------------------------------------------+
void I2S_IRQHandler(void) {
    // If the RX pointer was just updated, swap to the other buffer.
    if (NRF_I2S->EVENTS_RXPTRUPD) {
        extern volatile uint32_t i2s_irqs;
        extern volatile int16_t *i2s_buffer;
        extern volatile bool i2s_ready;

        // If the buffer number was 0, load buffer number 1, and
        // indicate that buffer 1 is now ready. Conversely, if the
        // buffer number was 1, load buffer 0 and indicate buffer 0
        // is ready.
        //
        // This is somewhat counterintuitive, because we're indicating
        // the buffer that was just loaded is ready. However, this is
        // because the interrupt fires before the frame starts transmitting.
        // That is, the first time this ISR runs, `buffer_num` is 0, indicating
        // the hardware just loaded buffer 0 into the register, meaning it will
        // start overwriting buffer 0 immediately.
        //
        // In this case, we must load buffer 1 into the register. Additionally,
        // since buffer 0 is getting written by the hardware, we must indicate
        // that buffer 1 is ready.
        buffer_num += 1;
        size_t next_buffer = buffer_num & 1;
        NRF_I2S->RXD.PTR = buffers[next_buffer];
        NRF_I2S->EVENTS_RXPTRUPD = 0;

        i2s_irqs++;
        i2s_buffer = (int16_t *)(buffers[next_buffer]);
        i2s_ready = true;
    }
}
