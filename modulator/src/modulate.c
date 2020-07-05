#include <math.h>
#include "modulate.h"

extern const char *char_to_varcode(char c);

void modulate_init(struct modulate_state *state, uint32_t carrier,
                   uint32_t rate, const char *str) {
    state->cfg.rate = rate;
    state->cfg.carrier = carrier;
    state->cfg.pll_incr = (float)carrier / (float)rate;
    state->cfg.omega = (2.0 * M_PI * carrier) / rate;
    state->cfg.string = str;

    state->bit_pll = 0.0;
    state->baud_pll = 0.0;
    state->polarity = 0;
    state->str_pos = 0;
    state->varcode_pos = 0;
    state->varcode_str = ""; // Default to an empty string
    state->bit_count = -1;
    state->run = 1;
    state->carrier_count = -1;

    return;
}

#ifdef MOD_LOG
// Dump this to disk with:
// (gdb) dump memory raw.bin mod_block mod_block+sizeof(mod_block)
// Then use `unpack.c` to convert it to a wav file:
// $ gcc unpack.c -o unpack
// $ ./unpack # creates "decoded.wav"
__attribute__((used)) uint8_t mod_block[65536];
uint32_t mod_block_offset;
uint32_t mod_block_bit;
#endif

static void send_bit(struct modulate_state *state, int bit) {
    state->polarity ^= !bit;
    state->bit_count = 0;
    state->carrier_count = 0;
}

#define ZERO_RUN 32
/// This loop is called when the PWM engine finishes a bit, and therefore
/// must generate the next bit.
int modulate_next_sample(struct modulate_state *state) {
    int pwm_state;

    if(!state->run) {
      return 0;
    }

#if MOD_METHOD == MOD_GPIO
    if (state->bit_count >= 32) {
#else if MOD_METHOD == MOD_PWM
    if (state->carrier_count >= 2) {
#endif
      
        // Find the next bit
        int next_bit = 0;
        if (state->varcode_pos < 0) {
            // Send inter-symbol (or initial training) zeroes
            state->varcode_pos++;
        } else if (!state->cfg.string[state->str_pos]) {
            // We've finished the string. Loop back around and start sending
            // sync pulses again.
            state->str_pos = 0;
            state->varcode_pos = -ZERO_RUN;
            state->varcode_str = char_to_varcode(state->cfg.string[0]);
 	    // state->run = 0; // uncomment for discontinous operation
        } else if (!state->varcode_str[state->varcode_pos]) {
            // Advance to the next symbol in the string.
            state->varcode_pos = -2; // Send two bytes of 0
            state->varcode_str =
                char_to_varcode(state->cfg.string[state->str_pos++]);
        } else {
            next_bit = (state->varcode_str[state->varcode_pos++] != '0');
        }
        send_bit(state, next_bit);
    }

#if MOD_METHOD == MOD_GPIO
    if (state->baud_pll > 0.5) {
        pwm_state = state->polarity;
    } else {
        pwm_state = !state->polarity;
    }

    state->baud_pll += state->cfg.pll_incr;
    if (state->baud_pll > 1.0) {
        state->bit_count++;
        state->baud_pll -= 1.0;
    }
#else if MOD_METHOD == MOD_PWM
    state->carrier_count++;
    pwm_state = state->polarity;
#endif

#ifdef MOD_LOG
    // Log to a memory buffer
    mod_block[mod_block_offset] |= (pwm_state << mod_block_bit++);
    if (mod_block_bit >= 8) {
        mod_block_bit = 0;
        mod_block_offset++;
        mod_block[mod_block_offset] = 0;
    }
    if (mod_block_offset >= sizeof(mod_block)) {
        mod_block_offset = 0;
    }
#endif

    return pwm_state;
}
