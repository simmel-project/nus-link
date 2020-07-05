#ifndef __ORCHARD_DEMOD__
#define __ORCHARD_DEMOD__

#include "arm_math.h"

typedef int16_t demod_sample_t;
struct bpsk_state;

#define SAMPLE_RATE 62500
#define CARRIER_TONE 20840
#define BAUD_RATE (651.0417f) // 31.25
#define PLL_INCR (BAUD_RATE / (float32_t)(SAMPLE_RATE))
#define SAMPLES_PER_PERIOD 20 // Must evenly divide CARRIER_TONE

void bpsk_demod_init(void);
int bpsk_demod(int *bit, demod_sample_t *samples, uint32_t nb,
               uint32_t *processed_samples);

#endif
