#ifndef __BPSK_H__
#define __BPSK_H__

#include <stdint.h>

void bpsk_init(void);
void bpsk_run(int32_t *samples, uint32_t nsamples);

#endif /* __BPSK_H__ */
