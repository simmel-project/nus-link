#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "arm_math.h"

#include "demod.h"
#include "mac.h"

#ifdef PLAYGROUND
#include "../printf.h"
#else
#include <stdio.h>
#endif

#include "fir_coefficients.h"
#include "lpf_coefficients.h"

struct nco_state
{
    float32_t samplerate; // Hz
    float32_t freq;       // Hz

    float32_t phase; // rad
    float32_t offset;
    float32_t error;
};

struct bpsk_state
{
    /// Convert incoming q15 values into f32 values into this
    /// buffer, so that we can keep track of it between calls.
    float32_t current[SAMPLES_PER_PERIOD];
    uint32_t current_offset;

    /// If there aren't enough samples to convert data from q15
    /// to f32, store them in here and return.
    demod_sample_t cache[SAMPLES_PER_PERIOD];
    uint32_t cache_capacity;

    arm_fir_instance_f32 fir;
    float32_t fir_state[SAMPLES_PER_PERIOD + FIR_STAGES - 1];

    struct nco_state nco;

    arm_fir_instance_f32 i_lpf;
    float32_t i_lpf_state[SAMPLES_PER_PERIOD + LPF_STAGES - 1];

    arm_fir_instance_f32 q_lpf;
    float32_t q_lpf_state[SAMPLES_PER_PERIOD + LPF_STAGES - 1];

    float32_t i_lpf_samples[SAMPLES_PER_PERIOD];

    float32_t agc;
    float32_t agc_step;
    float32_t agc_target_hi;
    float32_t agc_target_low;

    // Signal decoding
    float32_t bit_pll;
    float32_t pll_incr;
    int bit_acc;
    int last_state;
};

static struct bpsk_state bpsk_state;

extern char varcode_to_char(uint32_t c);
static void print_char(uint32_t c)
{
    printf("%c", varcode_to_char(c >> 2));
}

static void make_nco(float32_t *i, float32_t *q)
{
    // control is a number from +1 to -1, which translates to -pi to +pi
    // additional phase per time step timestep is the current time step,
    // expressed in terms of samples since t=0 i is a pointer to the in-phase
    // return value q is a pointer to the quadrature return value

    float32_t timestep = bpsk_state.nco.offset;
    while (timestep < SAMPLES_PER_PERIOD + bpsk_state.nco.offset)
    {
        bpsk_state.nco.phase += (bpsk_state.nco.error / PI);
        *i++ = arm_cos_f32((timestep * bpsk_state.nco.freq * 2 * PI) /
                               bpsk_state.nco.samplerate +
                           bpsk_state.nco.phase);
        *q++ = arm_sin_f32((timestep * bpsk_state.nco.freq * 2 * PI) /
                               bpsk_state.nco.samplerate +
                           bpsk_state.nco.phase);
        timestep += 1;
    }

    // XXX MAKE SURE TO DEAL WITH TIMESTEP WRPAPING
    if (timestep > bpsk_state.nco.samplerate)
        timestep -= bpsk_state.nco.samplerate;

    bpsk_state.nco.offset = timestep;
}

void bpsk_demod_init(void)
{
    arm_fir_init_f32(&bpsk_state.fir, FIR_STAGES, fir_coefficients,
                     bpsk_state.fir_state, SAMPLES_PER_PERIOD);

    bpsk_state.nco.samplerate = SAMPLE_RATE;
    bpsk_state.nco.freq = CARRIER_TONE;
    bpsk_state.nco.phase = 0.0;
    bpsk_state.nco.error = 0.0;
    bpsk_state.nco.offset = 0;

    bpsk_state.agc = 1.0;
    bpsk_state.agc_step = 0.05;
    bpsk_state.agc_target_hi = 0.5;
    bpsk_state.agc_target_low = 0.25;

    // Force a buffer refill for the first iteration
    bpsk_state.current_offset = SAMPLES_PER_PERIOD;

    bpsk_state.cache_capacity = 0;

    arm_fir_init_f32(&bpsk_state.i_lpf, LPF_STAGES, lpf_coefficients,
                     bpsk_state.i_lpf_state, SAMPLES_PER_PERIOD);
    arm_fir_init_f32(&bpsk_state.q_lpf, LPF_STAGES, lpf_coefficients,
                     bpsk_state.q_lpf_state, SAMPLES_PER_PERIOD);

    bpsk_state.bit_pll = 0.0;
    bpsk_state.pll_incr = ((float)BAUD_RATE / (float)SAMPLE_RATE);
    bpsk_state.bit_acc = 0;
    bpsk_state.last_state = 0;
}

// Dump this with:
// ```
//  dump binary memory sample.wav &sample_wave
//  ((uint32_t)&sample_wave)+sizeof(sample_wave)
// ```
#ifdef CAPTURE_BUFFER
#define CAPTURE_BUFFER_COUNT (32768)
struct sample_wave
{
    uint8_t header[44];
    uint16_t saved_samples[CAPTURE_BUFFER_COUNT];
};
struct sample_wave sample_wave;
uint32_t saved_samples_ptr;
#endif

// static FILE *output = NULL;
// static void write_wav_stereo(int16_t *left, int16_t *right, unsigned int len,
//                              const char *name)
// {
//     if (!output)
//     {
//         static uint8_t wav_header[44] = {
//             0x52,
//             0x49,
//             0x46,
//             0x46,
//             0x1c,
//             0x12,
//             0x05,
//             0x00,
//             0x57,
//             0x41,
//             0x56,
//             0x45,
//             0x66,
//             0x6d,
//             0x74,
//             0x20,
//             0x10,
//             0x00,
//             0x00,
//             0x00,
//             0x01,
//             0x00,
//             0x02, // stereo
//             0x00,
//             0x11,
//             0x2b,
//             0x00,
//             0x00,
//             0x22,
//             0x56,
//             0x00,
//             0x00,
//             0x02,
//             0x00,
//             0x10,
//             0x00,
//             0x64,
//             0x61,
//             0x74,
//             0x61,
//             0xf8,
//             0x11,
//             0x05,
//             0x00,
//         };

//         output = fopen(name, "w+b");
//         if (!output)
//         {
//             perror("unable to open filtered file");
//             return;
//         }
//         if (fwrite(wav_header, sizeof(wav_header), 1, output) <= 0)
//         {
//             perror("error");
//             fclose(output);
//             return;
//         }
//     }
//     for (unsigned int i = 0; i < len; i++)
//     {
//         if (fwrite(&(left[i]), 2, 1, output) != 1)
//         {
//             perror("error");
//             fclose(output);
//             return;
//         }
//         if (fwrite(&(right[i]), 2, 1, output) != 1)
//         {
//             perror("error");
//             fclose(output);
//             return;
//         }
//     }
// }

static void bpsk_core(void)
{
    // Perform an initial FIR filter on the samples. This creates a LPF.
    // This line may be omitted for testing.
    arm_fir_f32(&bpsk_state.fir, bpsk_state.current, bpsk_state.current,
                SAMPLES_PER_PERIOD);

    // Scan for agc value. note values are normalized to +1.0/-1.0.
    int above_hi = 0;
    int above_low = 0;
    for (int i = 0; i < SAMPLES_PER_PERIOD; i++)
    {
        bpsk_state.current[i] = bpsk_state.current[i] * bpsk_state.agc; // compute the agc

        // then check if we're out of bounds
        if (bpsk_state.current[i] > bpsk_state.agc_target_low)
        {
            above_low = 1;
        }
        if (bpsk_state.current[i] > bpsk_state.agc_target_hi)
        {
            above_hi = 1;
        }
    }
    if (above_hi)
    {
        bpsk_state.agc = bpsk_state.agc * (1.0 - bpsk_state.agc_step);
    }
    else if (!above_low)
    {
        bpsk_state.agc = bpsk_state.agc * (1.0 + bpsk_state.agc_step);
    }

    // Generate Numerically-Controlled Oscillator values for the
    // current timestamp.
    float32_t i_samps[SAMPLES_PER_PERIOD];
    float32_t q_samps[SAMPLES_PER_PERIOD];
    make_nco(i_samps, q_samps);

    static float32_t i_mult_samps[SAMPLES_PER_PERIOD];
    static float32_t q_mult_samps[SAMPLES_PER_PERIOD];
    arm_mult_f32(bpsk_state.current, i_samps, i_mult_samps, SAMPLES_PER_PERIOD);
    arm_mult_f32(bpsk_state.current, q_samps, q_mult_samps, SAMPLES_PER_PERIOD);

    // static float32_t bpsk_state.i_lpf_samples[SAMPLES_PER_PERIOD];
    static float32_t q_lpf_samples[SAMPLES_PER_PERIOD];
    arm_fir_f32(&bpsk_state.i_lpf, i_mult_samps, bpsk_state.i_lpf_samples, SAMPLES_PER_PERIOD);
    arm_fir_f32(&bpsk_state.q_lpf, q_mult_samps, q_lpf_samples, SAMPLES_PER_PERIOD);

    // int16_t i_loop[SAMPLES_PER_PERIOD];
    // int16_t q_loop[SAMPLES_PER_PERIOD];
    // arm_float_to_q15(bpsk_state.i_lpf_samples, i_loop, SAMPLES_PER_PERIOD);
    // arm_float_to_q15(q_lpf_samples, q_loop, SAMPLES_PER_PERIOD);
    // write_wav_stereo(i_loop, q_loop, SAMPLES_PER_PERIOD, "quadrature_loop.wav");

    float32_t errorwindow[SAMPLES_PER_PERIOD];
    arm_mult_f32(bpsk_state.i_lpf_samples, q_lpf_samples, errorwindow,
                 SAMPLES_PER_PERIOD);
    float32_t avg = 0;
    for (int i = 0; i < SAMPLES_PER_PERIOD; i++)
    {
        avg += errorwindow[i];
    }
    avg /= ((float32_t)SAMPLES_PER_PERIOD);
    bpsk_state.nco.error = -(avg);
    // printf("err: %0.04f\n", bpsk_state.nco.error);
}

int bpsk_demod(int *bit, demod_sample_t *samples, uint32_t nb, uint32_t *processed_samples)
{
    *processed_samples = 0;
    while (1)
    {
        // If we've run out of samples, fill the sample buffer
        bpsk_state.current_offset += 1;
        if (bpsk_state.current_offset >= SAMPLES_PER_PERIOD)
        {
            // If there aren't any bytes remaining to process, return.
            if (nb == 0) {
                return 0;
            }
            // If there's data in the cache buffer, use that as the source
            // for data.
            else if (bpsk_state.cache_capacity > 0)
            {
                // If there won't be enough data to process, copy the
                // remainder to the cache and return.
                if (bpsk_state.cache_capacity + nb < SAMPLES_PER_PERIOD)
                {
                    printf("Buffer not big enough, stashing in cache\n");
                    memcpy(&bpsk_state.cache[bpsk_state.cache_capacity],
                           samples, nb * sizeof(uint16_t));
                    bpsk_state.cache_capacity += nb;
                    *processed_samples += nb;
                    // fclose(output);
                    return 0;
                }

                // There is enough data if we combine the cache with fresh data,
                // so determine how many samples to take.
                uint32_t samples_to_take = (SAMPLES_PER_PERIOD - bpsk_state.cache_capacity);

                // printf("Pulling %d samples from cache and adding %d samples from pool of %d\n",
                //     bpsk_state.cache_capacity,
                //     samples_to_take, nb);
                memcpy(&bpsk_state.cache[bpsk_state.cache_capacity], samples,
                       samples_to_take * sizeof(uint16_t));

                // There is enough data, so convert it to f32 and clear the cache
                arm_q15_to_float(bpsk_state.cache, bpsk_state.current,
                                 SAMPLES_PER_PERIOD);
                bpsk_state.cache_capacity = 0;

                nb -= samples_to_take;
                samples += samples_to_take;
                (*processed_samples) += samples_to_take;
            }
            // Otherwise, the cache is empty, so operate directly on sample data
            else
            {
                // If there isn't enough data to operate on, store it in the
                // cache.
                if (nb < SAMPLES_PER_PERIOD)
                {
                    // printf("Only %d samples left, stashing in cache\n", nb);
                    memcpy(bpsk_state.cache, samples, nb * sizeof(uint16_t));
                    bpsk_state.cache_capacity = nb;
                    (*processed_samples) += nb;
                    return 0;
                }

                // Directly convert the sample data to f32
                arm_q15_to_float(samples, bpsk_state.current, SAMPLES_PER_PERIOD);
                nb -= SAMPLES_PER_PERIOD;
                samples += SAMPLES_PER_PERIOD;
                (*processed_samples) += SAMPLES_PER_PERIOD;
            }

            bpsk_core();
            bpsk_state.current_offset = 0;
        }


        // If the PLL crosses the 50% threshold, indicate a new bit.
        if (bpsk_state.bit_pll < 0.5 && (bpsk_state.bit_pll + bpsk_state.pll_incr) >= 0.5)
        {
            int state = bpsk_state.i_lpf_samples[bpsk_state.current_offset] > 0.0;
            *bit = !(state ^ bpsk_state.last_state);
            bpsk_state.last_state = state;

            bpsk_state.bit_acc = (bpsk_state.bit_acc << 1) | *bit;
            if ((bpsk_state.bit_acc & 3) == 0)
            {
                print_char(bpsk_state.bit_acc);
                bpsk_state.bit_acc = 0;
            }
        }

        // Advance the PLL, looping it when it passes 1
        bpsk_state.bit_pll += bpsk_state.pll_incr;
        if (bpsk_state.bit_pll >= 1)
        {
            bpsk_state.bit_pll -= 1;
        }
    }
}
