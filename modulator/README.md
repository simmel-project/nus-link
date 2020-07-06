# Modulator

This repo contains the code for the Near Ultrasound (NUS) modulator on Simmel.

It is designed to transmit a loop of a fixed sequence, for link characterization purposes. 

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

## Technical Description

Please refer to the root
[README](https://github.com/simmel-project/nus-link/blob/master/README.md)
for a description of how we arrived at the overall link parameters.

The modulator itself is conceptually simple: create a continuous waveform
at 20833.33Hz. When transitioning from a `0` to a `1`, simply invert the
waveform, to introduce the desired `pi` phase shift.

The modulator implementation is complicated by the fact that the
transducer is a piezo element. A piezo element is non-linear, in that
it is either `on` or `off`: there is no middle state, unlike a speaker
which has a voice coil that can be set, with great precision, to an
arbitrary amplitude.

Thus, we can only produce a square wave. At 20.833kHz, a square wave will create
harmonics, but the harmonics will be higher mulitples and thus inaudible;
furthermore, the anti-aliasing filter of the sampling microphone will eliminate
the higher harmonics, leaving us with a sine wave.

So far, so good!

The problem is, an abrupt phase transition causes a sideband. In our
case, the baud rate is 651Hz, which means the transitions representing
the bits are solidly in the audible range. The
[bpsk-modem](https://github.com/simmel-project/bpsk-modem) repo
contains code that will actually generate a `.wav` file, which [you
can listen
to](https://github.com/simmel-project/bpsk-modem/blob/master/samples/modulated.wav)
which demonstrates this problem. Basically, the modulated bits sound
like a low grumbling.

In an analog system, this can be solved by using a high-pass filter to remove
the sideband. [This sample](https://github.com/simmel-project/bpsk-modem/blob/master/samples/modulated-20833-hpf2.wav)
demonstrates the same waveform, but run twice through an Audacity high-pass
filter with the cutoff set to 20kHz and a 48dB per octaive roll-off. You
won't hear anything, but the data is still there!

## Eliminating Sidebands with a Non-Linear Transducer Element

Unfortunately, we have a non-linear transducer, therefore, we can't play
this filtering trick. Instead, we use wave-table synthesis to
gradually lengthen or shorten the period of the carrier. The NRF52
has a PWM unit which can "easyDMA" a sequence of memory to the PWM
modulator. In our implementation, we set up a 16-entry table which
contains half a baud's worth of wave data (a baud is 32 cycles). Each
table entry can define both the duty cycle of the PWM as well as the
duration of the period. 

Put in solid numbers, we clock the PWM at 4MHz (a period of 25ns), and
set the duration to 192 counts x 25ns = 20833.33Hz with a duty cycle
of 50% (thus a switchover at 96 counts) to create a carrier tone. We
call this the "same" case (a 0->0, or 1->1 bit will just look like a
constant carrier tone).

Whenever we go from a 0->1, or a 1->0, we need to "steal" or "give" 96
PWM counts of time to create a `pi` phase shift. The exact mapping of
steal or give to which transition is actually arbitrary, and is a
feature of the BPSK and Varicode coding (in that all you care about is
transitions and not the polarity; the demodulator mathematically
cannot actually say for sure what the polarity is, it can only find
transitions).

In order to reduce our sideband noise, we thus gradually add or remove
time equivalent to 96 counts over a period of a few carrier
cycles. You can see this in `src/i2s.c`:
https://github.com/simmel-project/nus-link/blob/443f15c60cba2515f64ab8de5c92e6c851b1fe45/modulator/src/i2s.c#L85

The actual amounts chosen was just eyeballed using a spreadsheet and
jammed into the C file; there are two options, one for a sharper
transition that is more audible, and one that is more gradual. The
two options were created because the rate of modulation pulls the
feedback loop in the demodulator, and the two options allow us to
help characetrize that interaction.

Thus the overall top-down organization of the modulator is as follows:

1. `modulate_next_sample()` inside `src/modulate.c` contains the state
machine which fetches characters from the test string, decodes them
into a PSK31 Varicode array, and then determines which bit is to be
sent next. 
1. `modulate_next_sample()` is driven by interrupts coming from
the `PWM0_IRQHandler` inside `src/i2s.c`. One interrupt fires
every 16 carrier cycles. The interrupt handler keepst track of
which bit was just sent, versus the next bit being requsted,
and applies the appropriate wave table (same-same, 0->1, or 1->0).

