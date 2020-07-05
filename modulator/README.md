# Playground: Near Ultrasound

This repo contains playground code for testing Near UltraSound (NUS) support on Simmel. It is designed to be used via a USB serial console.

This program is larger than the normal bootloader, so it will erase your existing bootloader as well as your circuit python install, so after you're done in the playground you will need to reflash the bootloader and reinstall circuit python.


## Usage

To use the NUS playground, enter this directory and run the following:

``` sh
$ git submodule init
$ git submodule update
$ make
$ arm-non-eabi-gdb _build/nus-test-nosd.elf -ex 'tar rem your-openocd-server:3333'
(gdb)
```

You can then make changes and reload in gdb:

``` 
(gdb) make
(gdb) load
(gdb) mon reset halt
(gdb) continue
```

## Setting AFSK parameters

You can use the [nus-harness](https://github.com/simmel-project/nus-harness/) to select frequency parameters that are conducive to this particular modulation algorithm. For example, to sweep through baud rates and filter widths, you might try running:

```sh
[3:03:42 PM] D:/Code/Simmel/nus-harness> cargo run --release -- --output ./log.csv --input ./test/rand.bin --baud 5000..49..5200 -r 62500 -h 17000 -l 15000 --filter-width 7..10
    Finished release [optimized + debuginfo] target(s) in 0.17s
     Running `target/release/nus-harness.exe --output ./log.csv --input ./test/rand.bin --baud 5000..49..5200 -r 62500 -h 17000 -l 15000 --filter-width 7..10`
Modulating ./test/rand.bin into ./log.csv.
Is update? false  Data rate: High  Protocol version: V2
PARAMETERS   baud_rate: 5000    f_lo: 15000   f_hi: 17000   filter_width: 7   DEMOD  99/102 97.059%
PARAMETERS   baud_rate: 5000    f_lo: 15000   f_hi: 17000   filter_width: 8   DEMOD  99/102 97.059%
PARAMETERS   baud_rate: 5000    f_lo: 15000   f_hi: 17000   filter_width: 9   DEMOD  99/102 97.059%
PARAMETERS   baud_rate: 5000    f_lo: 15000   f_hi: 17000   filter_width: 10  DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5049    f_lo: 15000   f_hi: 17000   filter_width: 7   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5049    f_lo: 15000   f_hi: 17000   filter_width: 8   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5049    f_lo: 15000   f_hi: 17000   filter_width: 9   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5049    f_lo: 15000   f_hi: 17000   filter_width: 10  DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5098    f_lo: 15000   f_hi: 17000   filter_width: 7   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5098    f_lo: 15000   f_hi: 17000   filter_width: 8   DEMOD  100/102 98.039%
PARAMETERS   baud_rate: 5098    f_lo: 15000   f_hi: 17000   filter_width: 9   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5098    f_lo: 15000   f_hi: 17000   filter_width: 10  DEMOD  97/102 95.098%
PARAMETERS   baud_rate: 5147    f_lo: 15000   f_hi: 17000   filter_width: 7   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5147    f_lo: 15000   f_hi: 17000   filter_width: 8   DEMOD  99/102 97.059%
PARAMETERS   baud_rate: 5147    f_lo: 15000   f_hi: 17000   filter_width: 9   DEMOD  102/102 100.000%
PARAMETERS   baud_rate: 5147    f_lo: 15000   f_hi: 17000   filter_width: 10  DEMOD  90/102 88.235%
PARAMETERS   baud_rate: 5196    f_lo: 15000   f_hi: 17000   filter_width: 7   DEMOD  88/102 86.275%
PARAMETERS   baud_rate: 5196    f_lo: 15000   f_hi: 17000   filter_width: 8   DEMOD  55/102 53.922%
PARAMETERS   baud_rate: 5196    f_lo: 15000   f_hi: 17000   filter_width: 9   DEMOD  98/102 96.078%
PARAMETERS   baud_rate: 5196    f_lo: 15000   f_hi: 17000   filter_width: 10  DEMOD  64/102 62.745%
[3:03:42 PM] D:/Code/Simmel/nus-harness>
```

From this output, you would know that 5049 baud is an ideal baudrate for this algorithm with the given F_LO and F_HI, and the filter_width can be made as low as 7.

These parameters can be set inside `src/main.c` in the `struct nus_config cfg` definition.
