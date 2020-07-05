#ifndef __MODULATE_H__
#define __MODULATE_H__

#include <stdint.h>

#define MOD_GPIO 0
#define MOD_PWM 1
#define MOD_METHOD MOD_PWM

struct modulate_cfg {
    uint32_t rate;
    uint32_t carrier;
    float pll_incr;
    float omega;
    const char *string;
};

struct modulate_state {
    struct modulate_cfg cfg;
    int run;
    float bit_pll;
    float baud_pll;
    int polarity;
    int str_pos;
    int varcode_pos;
    const char *varcode_str;
    int bit_count;    // count of samples elapsed (for GPIO mode)
    int carrier_count;  // count of carrier cycles elapsed (for PWM mode)
    int16_t high;
    int16_t low;
};

void modulate_init(struct modulate_state *state, uint32_t carrier,
                   uint32_t rate, const char *str);
int modulate_next_sample(struct modulate_state *state);

#endif /* __MODULATE_H__ */
