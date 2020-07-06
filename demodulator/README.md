# Demodulator

This repo contains the code for the Near Ultrasound (NUS) demodulator on Simmel.

Below is the TL;DR block diagram of the demodulator, mapping intermediate variables
onto locations in source files.

![demodulator block diagram](https://raw.githubusercontent.com/simmel-project/nus-link/master/simmel_demodulator.png)

## Usage

The Simmel development system assumes you already have an Rpi with the
appropriate adapter installed, and openocd compiled with BCM2835
support.

In order to start the openocd service, you will need to clone
the Simmel Scripts
(https://github.com/simmel-project/simmel-scripts.git) repository, and
run the `swd` script within. The README in the Simmel Scripts repo
will guide you through this.

You will also need to follow the directions within the
`simmel-scripts` repo to load the nrf52 "softdevice" firmware before
the modulator code can work. The softdevice firmware is part of the
`bootloader` repo.

Once you have loaded the softdevice firmware, and `swd` is running in the
background, you can program the test modulator with the following
commands:

``` sh
$ make
$ expect program.expect
```

In addition, you can extract a `.wav` file that allows you to view
snapshots of the processed samples. Again, assuming you have the
softdevice firmware and `swd` is running in the background, you can
extract the `.wav` file using the following command:

``` sh
$ make && ./get_sample.sh
```

This command set will make the current firmware, program it onto
the device, wait a few seconds for data to be collected, and then
write a stereo, 62.5kHz 16 bit per sample `.wav` file to `a_sample.wav`
in the current directory. You can copy this file over to a machine
that has [Audacity](https://www.audacityteam.org/) to view waves in
a format like this:

![example waveform](https://raw.githubusercontent.com/simmel-project/nus-link/master/demodulator/examples/simmel_demod_data_ideal_but_real_conditions.png)

## Technical Description

The demodulator is a [Costas Loop](https://en.wikipedia.org/wiki/Costas_loop).
It is a coherent detector, which means we do phase recovery and synchronization
of a local oscillator.

![demodulator block diagram](https://raw.githubusercontent.com/simmel-project/nus-link/master/simmel_demodulator.png)

The block diagram image is repeated here for viewing convenience. Here
is the path a sample takes from microphone to demodulated data:

`i2s.c` sets up the I2S block (see Technical Note below) to record
samples; `I2S_IRQHandler()` manages this, and sets `i2s_ready` every
time 2kiB of the double-buffered 4kiB sample buffer goes full. If
porting to another platform, this is probably the right API level to
divide the HAL from the demodulator loop implementation.

The main loop, `main.c/background_tasks()`, waits for `i2s_ready` to
be set by the interrupt handler. Once it is set, it will call
`bpsk.c/bpsk_run()` which in turn calls `bpsk/demod.c/bpsk_demod()`
and finally `bpsk/demod.c/bpsk_core()` (the various layers manage
real-time character printing, buffering, and sample slicing to the
native filter batch size).

`bpsk_core()` is where the DSP pipe is defined:

1. Incoming data is put through a high pass filter, to remove low frequency interfece
2. An AGC algorithm is run to normalize the input amplitude to the loop
3. The samples are split into I/Q arms by multiplying to the I/Q outputs of an NCO (numerically controlled oscillator)
4. The I/Q arms are low-pass filtered to remove the carrier
5. The arms are multiplied, and then filtered again to stabilize the loop. The error signal is then reduced through a "gain" block (-1.0 < gain < 0.0) and fed into the NCO's phase accumulation variable.
6. The "I" arm is then sent to a slicer which applies hysteresis and timing algorithms to extract raw `1`/`0` symbols that are then piped to a Varicode decoder

All of the filtering and math is done using the ARM CMSIS DSP library
(found in `src/cmsis/source`), which providese efficient primitives
for multiplication, IIR, FIR, and so forth using `f32` format, taking
advantage of the Cortex-M4's floating point DSP
instructions.

Significantly, there is a `blockSize` parameter (encoded as
`SAMPLES_PER_PERIOD` in `demod.c`) required across all the DSP
primitives. A larger `blockSize` primitive makes the DSP operations
more efficient, because the loops can be unrolled, but it also
introduces a big problem in feedback loops, that is, extra phase lag:
loop results are delayed by the length of the `blockSize` parameter,
and thus it subtracts phase margin from the feedback look, making it
unstable.

With a 64MHz Cortex-M4, a bit over half the CPU power is required to
implement the system using FIR filters with about 7 taps, and a
`blockSize` of 20. Unfortunately this `blockSize` is quite large
compared to the feedback loop closure rate, and we can only do a gross
"averaging" of the error signal; there isn't enough processing power
to implement a FIR with enough taps to get to a low enough frequency
to meaningfully filter the error signal.

Thus, the loop uses IIR filters. These are more compact, requiring
just 1 or 2 stages, but they suffer from numerical instability at
extreme cutoff values, and they introduce phase variations or gain
variations depending upon the form of the IIR filter (FIR filters, on
the other hand, can provide linear phase response and flat pass
bands). However, they allow the `blockSize` to be set to 2, which
keeps the amount of phase shift introduced into the loop to an
acceptable level for loop stability.

The v1 implementation uses the following parameters for the demodulator:

* The input HPF is an order-1 Bessel, with a passband of 15833Hz
* The AGC step is 0.005, with an amplitude convergence target of 0.2 - 0.35.
* The I/Q LPF filters are order-2 Butterworth, with a passband below 976Hz
* The loop LPF is omitted, and the gain is -0.05

The loop LPF is omitted in part due to computational limitations;
including it causes the loop to occassionally miss its computational
deadline. It feels like this might be due to a floating point exception
condition not being handled gracefully, but it is left as an open issue,
as the loop performs acceptable without the additional LPF.

## Thar Be Dragons: NRF52 Incompatibility with ICS-43434 Microphone

The NRF52 only supports I2S with frames up to 24 bits in length.

The ICS-43434 requires a 32-bit length stereo LRCLK frame, returinng
mono 16-bit samples. Seems weird, but the extra dummy clocks are
probably used by the internal digital filters on the ICS-43434:
attempts to shorten the frame length results in highly degraded
performance.

Thus, we cannot draw LRCLK directly from the NRF52's I2S block. Instead,
we dump the MCLK and LRCLK signals to unused pins. We tap SCLK directly
to the ICS-43434 and configure PWM0 to generate the LRCLK.

Then we "trick" the NRF52's I2S block and tell it that we are using 16
bits per sample, back-to-back, in stereo mode, but we generate an
LRCLK from the PWM0 module that toggles once every 32 samples.  The
resulting stream of data from the NRF's I2S block is thus [LN, RN,
LN+1, RN+1], where LN is the actual sample of interest, and RN, LN+1,
and RN+1 are garbage: the RN is the zero-pad on the microphone's
channel, and the "N+1" samples are sampling the 32-bits off-phase from
the microphone's channel. The re-formatting of data is handled here:
https://github.com/simmel-project/nus-link/blob/1a45980b6491df3ce41764d623a3204faf09c25c/demodulator/src/main.c#L190

Furthermore, the precise phase of the PWM and I2S blocks depends upon
the `TASKS_START` bit of the two blocks being set at precisely the right
time relative to each other. This is handled in the code here:
https://github.com/simmel-project/nus-link/blob/fd91188eb8d72c78ba306a8d0a8ad81ea91e5013/demodulator/src/i2s.c#L86

Moving around the lines of code, or adding or removing lines in between the
first and last lines, will break the I2S loop.

Too much magic!


