#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrfx.h"
#include "nrfx_i2s.h"
#include "nrfx_pwm.h"
#include "nrf_gpio.h"

#include "i2s.h"
#include "modulate.h"

static volatile uintptr_t buffers[2];
static volatile size_t buffer_size;
static volatile uint8_t buffer_num;
uint32_t samplecount = 1;
volatile uint32_t pwmstate = 0;


static uint16_t lrck_duty_cycles[PWM0_CH_NUM] = {32};

#define DOUBLE_POWER (96 | 0x8000)
#define SINGLE_POWER (0)
#define POWER_LEVEL DOUBLE_POWER

/*
Entries in the wave tables have the format of:

  PWM0 transition, PWM1 transition, [unused], total count

These are fetched in-order by the PWM sequencing engine. Once
the table is exhausted, an interrupt is fired and the PWM
engine is pointed at the next table, based upon the next bit's
value.

Note that the PWM clock is 4MHz, and 4MHz/192 = 20833.333Hz, 
which is the carrier frequency of the BPSK modulation.
 */
#if 0 // sharper transition
static uint16_t mod_duty_cycles_same[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
};
static uint16_t mod_duty_cycles_transition01[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 187,
					 96, POWER_LEVEL, 0, 182, // total shortening should be -192/2 = -96
					 96, POWER_LEVEL, 0, 174, // to give -pi phase shift to carrier
					 96, POWER_LEVEL, 0, 162,
					 96, POWER_LEVEL, 0, 174,
					 96, POWER_LEVEL, 0, 182,
					 96, POWER_LEVEL, 0, 187,
};
static uint16_t mod_duty_cycles_transition10[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 197,
					 96, POWER_LEVEL, 0, 202, // total lengthening should be 192/2 = 96
					 96, POWER_LEVEL, 0, 210, // to give +pi phase shift to carrier
					 96, POWER_LEVEL, 0, 222,
					 96, POWER_LEVEL, 0, 210,
					 96, POWER_LEVEL, 0, 202,
					 96, POWER_LEVEL, 0, 197,
};
#else // optimal quiet
static uint16_t mod_duty_cycles_same[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
};
static uint16_t mod_duty_cycles_transition01[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 191,
					 96, POWER_LEVEL, 0, 190,
					 96, POWER_LEVEL, 0, 188,
					 96, POWER_LEVEL, 0, 184,
					 96, POWER_LEVEL, 0, 179,
					 96, POWER_LEVEL, 0, 172,
					 96, POWER_LEVEL, 0, 172, // total shortening should be -192/2 = -96
					 96, POWER_LEVEL, 0, 179, // to give -pi phase shift to carrier
					 96, POWER_LEVEL, 0, 184,
					 96, POWER_LEVEL, 0, 188,
					 96, POWER_LEVEL, 0, 190,
					 96, POWER_LEVEL, 0, 191,
};
static uint16_t mod_duty_cycles_transition10[4*16] = {
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 192,
					 96, POWER_LEVEL, 0, 193,
					 96, POWER_LEVEL, 0, 194,
					 96, POWER_LEVEL, 0, 196,
					 96, POWER_LEVEL, 0, 200,
					 96, POWER_LEVEL, 0, 205,
					 96, POWER_LEVEL, 0, 212,
					 96, POWER_LEVEL, 0, 212, // total lengthening should be 192/2 = 96
					 96, POWER_LEVEL, 0, 205, // to give +pi phase shift to carrier
					 96, POWER_LEVEL, 0, 200,
					 96, POWER_LEVEL, 0, 196,
					 96, POWER_LEVEL, 0, 194,
					 96, POWER_LEVEL, 0, 193,
};
#endif

void nus_init(const struct i2s_pin_config *cfg, void *buffer, size_t len) {
    int i;

    // correct duty cycles based on total period (init values set a constant of 96)
    for( i = 0; i < 16; i++ ) {
      mod_duty_cycles_same[i * 4] = mod_duty_cycles_same[i * 4 + 3] / 2;
      mod_duty_cycles_transition01[i * 4] = mod_duty_cycles_transition01[i * 4 + 3] / 2;
      mod_duty_cycles_transition10[i * 4] = mod_duty_cycles_transition10[i * 4 + 3] / 2;
    }
    
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

    NRF_PWM_Type *pwm = NRF_PWM0;

    pwm->ENABLE = 0;

    pwm->PSEL.OUT[0] = cfg->word_select_pin_number;

    pwm->MODE = PWM_MODE_UPDOWN_Up;
    pwm->COUNTERTOP = 64;
    pwm->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_4; // 4 MHz, same as I2S
    pwm->DECODER = PWM_DECODER_LOAD_Individual;
    pwm->LOOP = 0;

    pwm->SEQ[0].PTR = (uint32_t)(lrck_duty_cycles);
    pwm->SEQ[0].CNT = 4; // default mode is Individual --> count must be 4
    pwm->SEQ[0].REFRESH = 0;
    pwm->SEQ[0].ENDDELAY = 0;

#if MOD_METHOD == MOD_GPIO
    pwm->INTENSET = NRF_PWM_INT_PWMPERIODEND_MASK; // PWMPERIODEND set
    pwm->EVENTS_PWMPERIODEND = 0;

    pwm->EVENTS_SEQEND[0] = 0;
    nrf_gpio_cfg_output(0 + 2);
    nrf_gpio_cfg_output(0 + 19);
    nrf_gpio_pin_set(0 + 2);
    nrf_gpio_pin_clear(0 + 19);
#endif
    pwm->ENABLE = 1;

#if MOD_METHOD == MOD_PWM
    NRF_PWM1->ENABLE = 0;
    NRF_PWM1->PSEL.OUT[0] = 0+2;
    NRF_PWM1->PSEL.OUT[1] = 0+19;

    NRF_PWM1->MODE = PWM_MODE_UPDOWN_Up;
    NRF_PWM1->COUNTERTOP = 192;
    NRF_PWM1->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_4; // 4 MHz, same as I2S
    NRF_PWM1->DECODER = PWM_DECODER_LOAD_WaveForm | PWM_DECODER_MODE_RefreshCount;
    NRF_PWM1->LOOP = 0;
    // NRF_PWM1->SHORTS = NRF_PWM_SHORT_LOOPSDONE_SEQSTART0_MASK; // when loop is done immediately start seq0 again

    NRF_PWM1->SEQ[0].PTR = (uint32_t)(mod_duty_cycles_same);
    NRF_PWM1->SEQ[0].CNT = 4*16;
    NRF_PWM1->SEQ[1].PTR = (uint32_t)(mod_duty_cycles_same);
    NRF_PWM1->SEQ[1].CNT = 0; // don't use
    NRF_PWM1->SEQ[0].REFRESH = 0;
    NRF_PWM1->SEQ[0].ENDDELAY = 0;

    NRF_PWM1->INTENSET = NRF_PWM_INT_SEQEND0_MASK; 
    NRF_PWM1->EVENTS_SEQEND[0] = 0;
    NRF_PWM1->ENABLE = 1;

    NRF_PWM1->TASKS_SEQSTART[0] = 1;

    /*
    NRF_PWM1->MODE = PWM_MODE_UPDOWN_Up;
    NRF_PWM1->COUNTERTOP = 768;
    NRF_PWM1->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_1; // 4 MHz, same as I2S
    NRF_PWM1->DECODER = PWM_DECODER_LOAD_Individual;
    NRF_PWM1->LOOP = 0;

    NRF_PWM1->SEQ[0].PTR = (uint32_t) &simple_test;
    NRF_PWM1->SEQ[0].CNT = 4;
    NRF_PWM1->SEQ[0].REFRESH = 0;
    NRF_PWM1->SEQ[0].ENDDELAY = 0;
    NRF_PWM1->INTENCLR = 0xFF;
    NRF_PWM1->ENABLE = 1;
    NRF_PWM1->TASKS_SEQSTART[0] = 1;
    */
    
#endif
    
}

void nus_start(void) {

    // Load buffer number 0 into the pointer.
    buffer_num = 0;
    NRF_I2S->RXD.PTR = buffers[buffer_num & 1];
    NRF_I2S->TXD.PTR = 0xFFFFFFFF;
    NRF_I2S->RXTXD.MAXCNT = buffer_size / 4; // In units of 32-bit words

    NVIC_SetPriority(I2S_IRQn, 3);
    NVIC_EnableIRQ(I2S_IRQn);

#if MOD_METHOD == MOD_GPIO
    NVIC_SetPriority(PWM0_IRQn, 2);
    NVIC_EnableIRQ(PWM0_IRQn);
#else if MOD_METHOD == MOD_PWM
    NVIC_SetPriority(PWM1_IRQn, 1);
    NVIC_EnableIRQ(PWM1_IRQn);
#endif

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
#if MOD_METHOD == MOD_GPIO
    NVIC_DisableIRQ(PWM0_IRQn);
#else if MOD_METHOD == MOD_PWM
    NVIC_DisableIRQ(PWM1_IRQn);
#endif
    
    NRF_I2S->TASKS_STOP = 1;
    NRF_I2S->CONFIG.RXEN = I2S_CONFIG_RXEN_RXEN_Disabled;
    NRF_I2S->EVENTS_RXPTRUPD = 0;
    NRF_I2S->RXTXD.MAXCNT = 0;

    NRF_I2S->CONFIG.TXEN = I2S_CONFIG_TXEN_TXEN_Enabled;

    NRF_I2S->INTENCLR = I2S_INTENSET_RXPTRUPD_Msk;
    NRF_I2S->ENABLE = I2S_ENABLE_ENABLE_Disabled;

    NRF_PWM0->TASKS_STOP = PWM_TASKS_STOP_TASKS_STOP_Trigger;
    NVIC_DisableIRQ(PWM0_IRQn);
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

extern struct modulate_state mod_instance;

#if MOD_METHOD == MOD_GPIO
static const uint32_t logical_0 = (0 << 19) | (1 << 2);
static const uint32_t logical_1 = (1 << 19) | (0 << 2);
static const uint32_t logical_mask = (1 << 19) | (1 << 2);

void PWM0_IRQHandler(void) {
    NRF_PWM_Type *pwm = NRF_PWM0;
#else if MOD_METHOD == MOD_PWM
void PWM1_IRQHandler(void) {
    NRF_PWM_Type *pwm = NRF_PWM1;
#endif
    if (pwm->EVENTS_PWMPERIODEND) {
        static int pwm_state = 0;
	static int last_pwm_state = 0;

#if MOD_METHOD == MOD_GPIO	
        uint32_t existing = nrf_gpio_port_out_read(NRF_P0) & ~logical_mask;
        if (pwm_state) {
            nrf_gpio_port_out_write(NRF_P0, existing | logical_0);
        } else {
            nrf_gpio_port_out_write(NRF_P0, existing | logical_1);
        }
	if( (samplecount % (62500 / 1 * 5)) == 1 ) {
	  mod_instance.run = 1;
	}
#else if MOD_METHOD == MOD_PWM
	
	pwm->SEQ[0].PTR = (uint32_t)(mod_duty_cycles_same);
	pwm->SEQ[0].CNT = 4*16;
	if (last_pwm_state == pwm_state) {
	  pwm->SEQ[0].PTR = (uint32_t)(mod_duty_cycles_same);
	  pwm->SEQ[0].CNT = 4*16;
	} else if(last_pwm_state == 0) {
	  pwm->SEQ[0].PTR = (uint32_t)(mod_duty_cycles_transition01);
	  pwm->SEQ[0].CNT = 4*16;
	} else {
	  pwm->SEQ[0].PTR = (uint32_t)(mod_duty_cycles_transition10);
	  pwm->SEQ[0].CNT = 4*16;
	}

	pwm->LOOP = 0;
	pwm->EVENTS_SEQEND[0] = 0;
	
	pwm->TASKS_SEQSTART[0] = 1;
	
	if( (samplecount % (62500 / 3 * 3)) == 1 ) { // delay modulator start by one second
	  mod_instance.run = 1;
	}
#endif
        samplecount = samplecount + 1;
	
        // compute next state in arrears, because the computation takes variable
        // time
	last_pwm_state = pwm_state;
        pwm_state = modulate_next_sample(&mod_instance);
    }
}
