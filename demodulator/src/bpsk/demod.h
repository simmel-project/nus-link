#ifndef __ORCHARD_DEMOD__
#define __ORCHARD_DEMOD__

#include "arm_math.h"

typedef int32_t demod_sample_t;
struct bpsk_state;

void bpsk_demod_init(void);
int bpsk_demod(uint32_t *bit, demod_sample_t *samples, uint32_t nb,
               uint32_t *processed_samples);

#endif
