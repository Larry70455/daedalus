# Running the PSP above 333 MHz

Sony's official maximum is 333 MHz for every PSP model, including the PSP Go,
and every model has 64 MB of RAM except the PSP-1000 (32 MB). DaedalusX64
already asks for that maximum (333 MHz CPU, 166 MHz bus), asks the firmware
for the full memory flag (`PARAM.SFO MEMSIZE=1`) and, on every model after the
PSP-1000, 4 MB of video memory.

Beyond 333 MHz is an unofficial hack that programs the clock PLL directly. The
project https://github.com/mcidclan/psp-beyond-444mhz (MIT licence) does this and
reports the PSP Go as tested. It is experimental: the speed a unit can reach
varies (the author reports some units pass 407 MHz, some sit near it, some stay
below), and an unstable setting crashes the console until it is rebooted.
Use it at your own risk. I have not tested it on a PSP.

## Recommended way: use that project's plugin or picker, not Daedalus code

The plugin changes the PLL registers itself and does not hook the power
functions, so Daedalus's normal start-up clock request does not undo it.

1. Build the "overclock-picker" (or the experimental plugin plus stress tester)
   from that repository and follow its README. The stress tester finds the
   highest stable frequency for your own console.
2. Start Daedalus as usual and toggle the overclock with the plugin's hotkey
   (L + R + NOTE for the experimental plugin).
3. Daedalus does not need to be told. Do not add a second overclock to it.

## Checking that it really took effect

`scePowerGetCpuClockFrequency` only reports what the system believes, and an
overclock plugin does not change that. Use the perf log build instead
(see PERF_LOG.md). Its first lines now include:

    # system: model PSP Go (kuKernelGetModel=4), 64MB model yes, system reports cpu 333 MHz bus 166 MHz
    # system: cpu speed test (20M loop iterations) 180841 us, ...

The second line times a fixed loop. Run a game once at 333 MHz and once with the
overclock active: the ratio of the two numbers is the real CPU speed up (for
example 180000 us down to 135000 us is 1.33 times faster, about 444 MHz).
