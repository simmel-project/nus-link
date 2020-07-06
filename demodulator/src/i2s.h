#ifndef __I2S_H__
#define __I2S_H__

#include <stdint.h>

struct i2s_pin_config {
  uint32_t data_pin_number;        // SDIN / SDOUT
  uint32_t bit_clock_pin_number;   // MCK
  uint32_t word_select_pin_number; // LRCK
};

void nus_init(const struct i2s_pin_config *cfg, void *buffer, size_t length);
void nus_start(void);
void nus_stop(void);

#endif /* __I2S_H__*/
