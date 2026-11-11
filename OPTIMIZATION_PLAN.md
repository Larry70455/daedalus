# Speed plan for DaedalusX64 on the PSP Go

Goal: N64 games at full speed, with audio, on a PSP Go.

Status: planning plus measurement tools. No speed changes have been made yet.
Everything below that says "PPSSPP" was measured on a PC with PPSSPP's headless
runner, not on a real PSP. See "What only real hardware can answer".

## 1. How things were measured

- `DAEDALUS_PERF_LOG` build writes per frame timings (see PERF_LOG.md).
- `DAEDALUS_STATE_CHECK` build runs a ROM headless and dumps CPU state per frame
  (see Tools/statecheck_ppsspp.sh).
- Games used: Super Mario 64 (US) and Super Smash Bros. (US), about 40 to 60
  seconds of title and intro scenes, no button presses.
- PPSSPP counts roughly one cycle per instruction and ignores cache misses, so
  real PSP numbers will be higher. Treat the shares as a guide and the
  absolute milliseconds as a lower bound.

## 2. Measurements so far (PPSSPP, audio off)

Time per N64 vertical blank (full speed needs 16.7 ms):

| | Mario 64 | Smash Bros |
|---|---|---|
| Total | 7.6 ms | 11.9 ms |
| Running recompiled code | 42% | 38% |
| Other CPU work (interpreter, block switching, events) | 37% | 45% |
| Display list plus vertex transform | 21% | 16% |
| Texture conversion | 0.1% | 0.6% |

Audio (PPSSPP, same scenes):

| Audio mode | Main CPU time per frame spent on audio | Note |
|---|---|---|
| Off (default on PSP) | 0 | |
| Sync (main CPU) | about 2.2 ms (12% of a frame) | measured |
| Async (Media Engine) | about 0.03 ms on the main CPU | PPSSPP has no ME firmware, so this is not trustworthy |

With audio on, the emulator waits 23 to 40% of each frame for the audio
buffer to drain. That is spare capacity (the game is paced by the audio), not a
cost. The wait is a busy spin loop inside `CAudioBuffer::AddSamples`
(`while (write_ptr == read_ptr)` on an uncached buffer). On a real PSP that
burns battery and keeps hitting the memory bus while the GPU and Media Engine
also need it. Replacing the spin with a short sleep would not change speed but
should help battery and bus contention. Low risk, not yet done.

Audio work by function (sync mode, function level profile): `ENVMIXER`,
`ADPCM`, `MIXER` and `Resample` are the real audio cost. For Mario 64 the
audio ucode is about 2.2 ms of about 11 ms of active CPU time per frame, so
roughly 20% of the work the CPU actually does. Smash Bros uses the `*3`
variants (`ENVMIXER3`, `MIXER3`, `ADPCM3`, `RESAMPLE3`), so both versions of
the mixer code need attention.

## 3. Corrections to earlier assumptions

- An earlier report said about 90% of time was inside compiled code. That was
  wrong. The RAM checksum in my own test tool was being counted there. Fixed
  with the `ramhash=0` option.
- "Event queue memmove costs 5%" was wrong. The queue has at most 4 entries and
  events are rare. The hot addresses were inside `memcmp`, a symbol table
  mislabel. The likely caller is the texture cache comparing `TextureInfo`
  structs. It is a small cost. Dropped from the plan.
- "Texture conversion on the Media Engine": textures are under 1% in the scenes
  measured. Dropped.
- Per audio job the code calls `sceKernelDcacheWritebackInvalidateAll`. On real
  hardware that is a 16 KB cache, so it is cheap. Replacing it with a range
  flush would not help (RDRAM is far larger than the cache). Dropped.

## 4. New findings that matter

1. **`LDC1` and `SDC1` are 99% of all dynarec fallbacks** (about 1,700 per
   frame in Smash). They are mostly saving and restoring FPU registers in
   function prologues and epilogues. The dynarec code for them exists but is
   switched off, and the `SDC1` version is wrong in two ways: it reads the
   wrong register (`ft+2` instead of `ft+1`), and it stores simulated doubles
   as raw words. The interpreter converts a simulated double back to a real
   one before storing; the dynarec version does not. Turning it on as is would
   corrupt memory. It needs a proper implementation, see item 3 below.
2. **Async audio has a race.** In `AudioPluginPSP::ProcessAList` the call
   `BeginME` returns -1 if the Media Engine is still busy with the previous
   job. The code then runs `Audio_Ucode()` on the main CPU while the Media
   Engine is still running the same code on the same shared state. That can
   corrupt audio. The fix is to wait for the Media Engine instead, but it
   cannot be tested without a real PSP.
3. **The PSP build asked for limited memory** (`PARAM.SFO MEMSIZE=2`). Fixed on
   this branch (`DAEDALUS_PSP_MEMSIZE=1`). Master crashed at startup in PPSSPP
   because of it.
4. **A clipping crash** (`NDCPlane` lost its 16 byte alignment when `v4` was
   replaced by `glm::vec4`) hits as soon as a game needs clipping. Fixed on
   this branch.

## 5. Ranked plan

Order is by expected gain for "full speed with audio" against risk.

| # | Change | Evidence | Expected gain | Risk | Status |
|---|---|---|---|---|---|
| 1 | Real hardware run of the PerfLog build with audio off, sync and async | PPSSPP cannot model cache and Media Engine behavior | Calibrates everything below | none | **requested** |
| 2 | Fix the Media Engine busy race (wait, do not run in parallel) | Section 4, item 2 | Correct async audio | low | planned, untestable here |
| 3 | Speed up the audio mixer inner loops (`ENVMIXER`, `ENVMIXER3`, `MIXER`, `MIXER3`, `ADPCM`, `ADPCM3`, `Resample`), for example with the VFPU or tighter integer code. Also replace the buffer wait spin with a short sleep | About 2.2 ms per frame (roughly 20% of active CPU time) in sync mode. Faster code helps sync mode and the Media Engine | 5 to 10% of a frame | low to medium (output must stay bit identical, which can be checked by comparing the audio buffer) | next |
| 4 | Make async audio the default if step 1 shows it works | Removes about 12% of the main CPU load | about 12% | medium | after 1 and 2 |
| 5 | Native `LDC1`/`SDC1` with simulated double handling | Section 4, item 1 | about 3 to 7% in Smash | medium to high | needs generated test ROMs |
| 6 | Cheaper compiled block exits and block linking | Block exit checks, block switching and return address lookup add up to about 25% of PSP code time in Smash, 10% in Mario (profile is rough) | 10 to 20% | high | after more data |
| 7 | Reduce interpreter time: compile hot code sooner, avoid flushing the whole code cache | About 10,000 interpreted instructions per frame in Smash | up to 20% in Smash | medium | after 6 |
| 8 | Restore the VFPU 4x4 matrix multiply lost in the GLM conversion | `glm::operator*` about 2.7% in Smash | about 1% of a frame | low | planned |
| 9 | Wider idle loop detection (skip more wait loops) | Only the exact pattern jump to self plus nop is skipped today | large for some games, zero for others | medium (timing) | needs per game data |
| 10 | Per game defaults in `roms.ini` (audio mode, frameskip, cycles per instruction) | Lets a CPU bound game drop its own frames and keep correct speed and sound | biggest playability lever on slow games | per game, off by default | after 1 |

## 6. How each change gets checked

- **Determinism:** two runs of the same build on the same ROM produce identical
  CPU state and RAM on all 600 frames checked. So a change that must not alter
  emulation results (audio speedups, exit and block changes, matrix multiply)
  can be checked by an exact before and after match.
- **Interpreter versus dynarec:** Tools/statecheck_ppsspp.sh runs a list of ROMs
  under both and compares final RAM and FPU state. 91 of 91 public domain CPU
  and FPU test ROMs pass. On real games the two diverge slowly because
  interrupts land a few instructions apart, so for dynarec changes the exact
  comparison is dynarec before versus dynarec after.
- **Dynarec coverage:** the public tests run almost nothing through the dynarec.
  A generated test ROM suite (random instruction sequences in hot loops, run
  under both cores) is needed before step 5 or 6.
- **Every change:** one small commit, a switch to turn it off, and a perf log
  number before and after. Anything not run on a real PSP is marked as such.

## 7. What only real hardware can answer

- How far real PSP timing is from PPSSPP's (cache misses, bus speed).
- Whether async audio works, and whether the race in section 4 ever triggers.
- GPU wait time, which PPSSPP reports as about zero.
- Whether `MEMSIZE=1` fixes crashes or slowness on the PSP Go under ARK-5.
- Whether running above 333 MHz is stable. The emulator sets 333 MHz at startup
  (`Source/SysPSP/main.cpp`), which is the official maximum.

Requested test: install the `DaedalusX64-PSP-PerfLog` build, play Mario 64 for
about two minutes in a 3D area with Audio set to Async, then Sync, then
Disabled (ROM settings screen), quit through the pause menu, and send the
`PerfLogs` folder with notes on sound quality and speed.


## 8. Results of the first execution round (PPSSPP, limiter off, audio off)

Emulated time per N64 frame, same instrumentation before and after each step.

| Step | Mario 64 | Smash Bros | Checked by |
|---|---|---|---|
| Baseline | 6545 us | 11302 us | |
| Restore VFPU matrix multiply | 6503 us | 11121 us | state identical (600 frames), screenshots pixel identical at 4 frame counts |
| Native LDC1/SDC1 | 6456 us | 10677 us | state and all RAM identical (600 frames), registers and FPU identical (3000 frames) |
| Total | about 1.4% faster | about 5.5% faster | |

Findings that change the plan:

- **The average hides a heavy tail.** Median frame is 2.8 ms (Mario) and 7.0 ms
  (Smash) but the 99th percentile is 19.7 ms and 50 ms, and 15% of Smash frames
  exceed the 16.7 ms budget. The spikes are what cause stutter and audio
  crackle, not the average.
- **Spike frames are cold code being interpreted**, not compiling: Smash spike
  frames run about 26,000 interpreted instructions (normal frames 5,000), costing
  about 8.7 ms, while code generation is only 1.5 ms of that. Compiling is 2 to
  5% of total time.
- **The code cache is thrown away completely 28 times in 3000 frames (Mario) and
  15 times (Smash)**: 3 from the CACHE instruction and the rest from writes that
  overlap compiled code. Each flush forces everything to be recompiled.
  Dropping only the overlapping fragments, instead of everything, is the best
  candidate for the next round, but fragments may be linked to each other, so it
  needs care.
- **Lowering the "hot" threshold (10 executions before compiling) is not safe.**
  At 4, Mario crashes inside generated code: compiling sooner exposes a latent
  bug where a load is assumed to be in fast RAM from too few observations.
- **Audio buffer:** 2048 samples (46 ms) ran dry whenever a frame took longer.
  In Sync mode over 40 seconds, underruns fell from 31 to 6 (Mario) with 4096
  samples. Smash stays at about 420 because in Sync mode it is simply too slow;
  only moving audio to the Media Engine fixes that.
- The `memmove` entry seen earlier was `memcmp` (texture info comparisons) and is
  negligible.

Not done yet: block linking (step 2), wider idle loop detection (4), display
list handler tuning (6), Media Engine audio by default (7, needs hardware),
per game cycle scaling (8), and the April 2023 comparison (9, that source does
not run in PPSSPP).
