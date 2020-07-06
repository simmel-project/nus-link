#!/bin/bash
expect program.expect
sleep 6
yes | gdb -ex 'file _build/nus-test-nosd.elf' -ex 'target remote localhost:3333' -ex 'dump binary memory a_sample.wav &sample_wave ((uint32_t)&sample_wave)+sizeof(sample_wave)' -ex 'quit'
expect run.expect
