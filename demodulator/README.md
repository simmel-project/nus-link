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


## Trick #2: NRF52 Incompatibility with ICS-43434 Microphone

The NRF52 only supports I2S with frames up to 24 bits in length.

The ICS-43434 requires a 32-bit length stereo LRCLK frame, returinng
mono 16-bit samples. Seems weird, but the extra dummy clocks are
probably used by the internal digital filters on the ICS-43434:
attempts to shorten the frame length results in highly degraded
performance.

Thus, we cannot draw LRCLK directly from the NRF52's I2S block. Instead,
we dump the MCLK and LRCLK signals to unused pins. We tap SCLK directly
to the ICS-43434 and configure PWM0 to generate the LRCLK.

Then we "trick" the NRF52's I2S block and tell it that we are using
16 bits per sample, back-to-back, in stereo mode, but we generate
an LRCLK from the PWM0 module that toggles once every 32 samples. 