# Measuring performance on the PSP

This explains how to run the measurement build, collect its log files, and
send them back. Nothing here changes how games run in the normal build.

## What the measurement build is

CI builds two PSP versions on every push:

| Artifact name             | Use it for                                   |
|---------------------------|----------------------------------------------|
| `DaedalusX64-PSP`         | Playing. The normal build.                   |
| `DaedalusX64-PSP-PerfLog` | Measuring. A little slower, writes log files. |

The PerfLog build is the same emulator compiled with `DAEDALUS_PERF_LOG=ON`.
It records where the time goes in every frame. Because it measures itself, it
runs slightly slower than the normal build, so don't judge speed by eye while
using it.

## 1. Install the PerfLog build

Download `DaedalusX64-PSP-PerfLog` from the Actions tab exactly as described
in [BUILD.md](BUILD.md). Install it the same way, but you can keep it separate
from your normal copy by renaming the folder, for example
`PSP/GAME/DaedalusX64-Perf/`. Copy the ROMs you want to test into its `Roms`
folder.

`BUILD_INFO.txt` inside the folder should say `DAEDALUS_PERF_LOG=ON`.

## 2. Run a game

1. Start the game from the ROM list as usual.
2. Play a normal section for **2 to 3 minutes**. Skip intros if you can. The
   first seconds are always slow because the dynarec is still translating
   code, so a short run gives misleading numbers.
3. Try to include a part that feels slow to you (for example a busy level or
   a menu that stutters).
4. Press **SELECT** to open the pause menu and choose **Return to Main Menu**.
   This writes the summary at the end of the log. If you quit with the HOME
   button instead, most of the log is still saved (it is written every few
   seconds), but the summary is missing.
5. Repeat for each game you care about. Each game gets its own files, and
   running a game again adds a new session to the end of the same file.

Pausing is counted as part of a frame, so avoid sitting in the pause menu
during a test.

## 3. Find the files

On the PSP, in the emulator folder:

```
PSP/GAME/DaedalusX64-Perf/PerfLogs/
    Super_Mario_64_635a2bff.csv             <- per frame timings
    Super_Mario_64_635a2bff_fallbacks.txt   <- dynarec fallback table
```

(The name is the game title with spaces and symbols turned into `_`, then the
ROM's checksum.)

Copy the `PerfLogs` folder to a computer over USB (Settings > USB Connection).

## 4. Send them back

Zip the `PerfLogs` folder and attach it in the chat, or attach the individual
files. They are plain text (the CSV grows by about 2 MB per 10 minutes of play;
it zips down to a fraction of that).

If attaching is hard, the most useful parts to paste as text are:

- the `# summary:` lines at the end of each `.csv` file, and
- the first 20 lines of each `_fallbacks.txt` file.

Please also say which game section you played and whether it felt slow.

## What the numbers mean

Each CSV line is one N64 vertical blank (60 per second for NTSC games, 50 for
PAL). All times are in microseconds (1/1,000,000 of a second). At 60 Hz one
vertical blank is 16,667 us, so if `total` is regularly much higher than that,
the game runs slower than full speed.

| Column        | Meaning                                                            |
|---------------|--------------------------------------------------------------------|
| `vi`          | Vertical blank number since the game started                       |
| `flips`       | New images the game showed during this vertical blank (0 or 1)     |
| `total`       | Time this vertical blank took                                      |
| `cpu_other`   | Interpreter, CPU events, memory handling: anything not listed below |
| `dynarec`     | Running recompiled code (includes fallbacks it calls)              |
| `dlist`       | Reading the N64 display list (excluding the next two)              |
| `vertex`      | Transforming, lighting and clipping vertices                       |
| `texture`     | Converting N64 textures to the PSP format                          |
| `gpu_wait`    | Waiting for the PSP graphics chip to finish                        |
| `audio_hle`   | Audio processing on the main CPU, or handing it to the Media Engine |
| `audio_wait`  | Waiting because the audio buffer is full                           |
| `limiter`     | Sleeping because the game is running too fast (idle, not a problem) |
| `log_io`      | Writing these log files                                            |
| `interp_ops`  | N64 instructions run by the plain interpreter                      |
| `fallback_ops`| Instructions the dynarec handed back to the interpreter            |

Each time is counted once, in the most specific bucket. For example, texture
conversion that happens while reading the display list is counted under
`texture`, not `dlist`.

The `_fallbacks.txt` file lists the instructions the dynarec could not
translate, most frequent first. `count` is how many times they ran, `share` is
the percentage of all fallbacks, and `sites` is how many places in the
translated code call them.

## Known limits

- The interpreter fallback **time** is not measured separately; it is included
  in `dynarec`. The fallback **count** per instruction is in the fallbacks
  file. Timing every fallback would add overhead in the hottest code.
- Audio processing on the Media Engine runs in parallel and is not timed. Only
  the main CPU side (`audio_hle`, `audio_wait`) is.
- The timer has a resolution of 1 microsecond.
- This has been tested only by compiling it and by unit tests on a PC. It has
  not yet been run on a real PSP.
