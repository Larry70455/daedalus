# Native code: precompiled guest functions (prototype)

Status: working prototype, tested in PPSSPP only (no PSP Go hardware run yet).

Daedalus can run selected N64 functions as code compiled ahead of time for the
PSP (Allegrex), built into the EBOOT. Everything else runs through the
existing dynarec and interpreter as before. Both kinds of code share the same
guest RAM, registers, cycle counter and interrupt handling.

The pipeline: an N64 function goes through N64Recomp (MIPS to C), then
`Tools/native_recomp/gen_module.py` (adapts the C to Daedalus), then the PSP
compiler, as part of the normal build.

## How it works

### Dispatch (`Source/Native/NativeCode.cpp`, `Source/Core/Dynamo.cpp`)

- Each generated module belongs to one ROM, identified by the header CRCs
  (the same ROM id as `roms.ini`). `Native_RomOpened` picks it when a ROM is
  loaded.
- When the dynarec dispatcher reaches an address with no compiled fragment,
  `Native_TryInsert` checks whether the module has a function starting there.
  If the guest code in RDRAM still has the checksum recorded at build time, a
  fragment is inserted that calls the native function. It is the same kind of
  fragment the OS HLE patches use, so it is linked to and from other
  fragments, looked up and dropped exactly like any other compiled code.
- If the checksum does not match (other code is loaded there), the address
  is compiled by the dynarec as usual.

### State and timing

- `native_execute` copies the guest registers (GPR, HI/LO, FPU pairs) into
  N64Recomp's context, runs the function and copies them back. Guest memory
  is shared: RDRAM is accessed in place (Daedalus and N64Recomp use the same
  word swapped layout); other addresses go through Daedalus's memory
  handlers.
- The generator puts `NATIVE_COUNT_OP()` after every guest instruction, so
  the cycle counter advances by exactly the number of instructions the guest
  code ran, as in the interpreter. Events that fall inside a native run (a
  vertical blank, a timer) are delivered when it returns, as with any
  compiled fragment.
- `jal`/`jalr` set `ra` to the real return address before the delay slot, as
  the CPU does (N64Recomp itself leaves `ra` alone). This matters: a callee
  saves `ra` on its stack, and a game can later read that stack slot.

### Invalidation

Native fragments live in the fragment cache, so DMA, overlay loads,
`osInvalICache` and the CACHE instruction drop them exactly as they drop
dynarec code. Before a native function is inserted again, its guest code is
compared with the build time checksum. Code that changed runs in the dynarec
from then on.

### Calls from native code back into Daedalus (fallback)

The generator gives every function the native code calls but that is not
native a small bridge, `native_call_guest`. It runs the callee the way the
dynarec would:

- If the callee is an OS function that Daedalus patches (osRecvMesg,
  osSendMesg, osInvalDCache, bcopy, osGetTime, osVirtualToPhysical, ...), the
  HLE patch runs. It gets the same arguments and leaves the same return value
  in v0, charges the same 100 cycles as a patch fragment, and queues
  completion events and interrupts through the normal CPU jobs.
- Otherwise the guest code is interpreted until it returns to the caller
  (the return address together with the caller's stack pointer identifies
  the return). Patched OS functions it calls on the way use their HLE
  patches, and interrupts and exceptions are taken as usual.

Calls through function pointers (`jalr`) go through the same bridge.

## Tools (`Tools/native_recomp`)

| File | What it does |
|---|---|
| `test.c`, `crt0.S`, `boot.S`, `link.ld`, `mkrom.py`, `make_rom.sh` | Homebrew test ROM. It has its own boot stub (no Nintendo IPL3), so it runs in emulators only. |
| `recomp.toml` | N64Recomp configuration: only `native_checksum` is recompiled. |
| `gen_module.py` | N64Recomp output to a Daedalus module: cycle counting, stores through Daedalus, `ra` on calls, bridges, module table with ROM id and code checksums. The module registers itself at startup. |
| `make_test_module.sh` | Runs the steps above for the test ROM and writes `Source/Native/Generated/native_test.c`. |
| `rom_to_elf.py` | Makes an ELF with symbols from functions of a commercial ROM (`sm64/functions.txt`), so N64Recomp can recompile them without a decompilation. |
| `find_candidates.py` | Lists functions of a ROM that are safe for this prototype (see Limits). |

Toolchain used: Ubuntu `gcc-mips-linux-gnu` (big endian MIPS, for the test
ROM and `rom_to_elf.py`), N64Recomp built from
https://github.com/N64Recompiled/N64Recomp (commit 598ce20), and the usual
pspdev toolchain for Daedalus.

### Test ROM module (in the repository)

```
N64RECOMP=/path/to/N64Recomp Tools/native_recomp/make_test_module.sh /tmp/nt
```

`Source/Native/Generated/native_test.c` is committed, so a normal Daedalus
build does not need N64Recomp. The ROM itself is `/tmp/nt/native_test.z64`.

### Modules for commercial games (generated locally)

Code generated from a commercial game is a translation of that game's code,
so it is not committed (`.gitignore` ignores `Source/Native/Generated/*.c`
except the test module). To build the Super Mario 64 (USA) module from your
own ROM:

```
cd Tools/native_recomp
python3 rom_to_elf.py sm64.z64 sm64/functions.txt /tmp/sm64n
cp sm64/recomp.toml /tmp/sm64n/ && (cd /tmp/sm64n && N64Recomp recomp.toml)
python3 gen_module.py sm64 /tmp/sm64n/recomp_out/funcs.c /tmp/sm64n/funcs.elf sm64.z64 ../../Source/Native/Generated/sm64.c
```

Then build Daedalus as usual (CMake picks up every file in `Generated/`).
`sm64/functions.txt` was made with `find_candidates.py` from a StateCheck run
with `oslist=1`.

## Test results (PPSSPP, dynarec)

State checks use `DAEDALUS_STATE_CHECK` builds; `native=0` in `statecheck.txt`
turns native code off for the comparison run.

| Test | Result |
|---|---|
| Test ROM, native code on: `native_checksum` native, `helper_mix` falls back | 50 native calls (46,950 guest instructions as native code), 400 calls back into the emulator (2,800 interpreted instructions), in one session |
| Same, compared with native code off (JIT only), 60 frames | Identical every frame: PC, cycle counter, all 32 registers, FPU, HI/LO, hash of all 8 MB of RDRAM, result words |
| Test ROM overwrites `native_checksum` with other code and invalidates the instruction cache | Checksum mismatch detected, new code runs in the dynarec, result equals the expected value of the new code |
| Super Mario 64 (USA), 7 native functions calling bcopy, memcpy, osGetTime, osVirtualToPhysical, 1500 frames | 28,966 native calls, 28,158 HLE calls from native code, no crash; final screenshot pixel identical to the JIT only run; state identical except frames 435 to 491 (a vertical blank delivered at the end of a native run, 7 cycles later), identical again from frame 492 |
| Super Mario 64, native code off, against the build this branch started from (1500 frames) | Identical every frame |
| Pokemon Stadium 2 (no native module) against the build this branch started from (2000 frames) | Identical every frame, same final screenshot |

One of the 51 test ROM calls before the overwrite runs in the interpreter
instead of natively: it happens while the dynarec switches to its trace
recorder, when Daedalus's dispatcher does not look up fragments (existing
behaviour for all compiled code). The results are unaffected.

## Limits (prototype)

- **Callees that switch threads.** If a function called from native code
  blocks (for example `osRecvMesg` waiting for a DMA, or `osPiStartDma`
  waking the PI manager thread), the native function stays suspended on the
  PSP stack while the interpreter runs the other guest threads. That works
  for a while, but Super Mario 64 with `dma_read` native hangs about 95
  frames in. Another thread later resumes from stale saved state; the cause
  is not found yet. `find_candidates.py` therefore only accepts functions
  whose calls never switch threads. Real fix: run each guest thread on its
  own PSP thread (as N64Recomp's own runtime does), or make native code return
  to the dispatcher at such calls.
- Native code is only dispatched by the dynarec core, like the OS patches.
  In interpreter only mode it is not used.
- Stores done by native code do not check for overwriting compiled code
  (same as Daedalus's normal stores; DMA and CACHE do invalidate).
- 8 and 16 bit stores to hardware registers go through Daedalus's plain
  memory path, without register side effects (32 bit stores use the full
  handlers).
- FPU code: registers are passed for the usual 32 bit FPU mode, but no
  native function with FPU instructions has been tested yet.
- Performance: the native functions tested so far are small, so no speed
  numbers are claimed. Not run on a real PSP Go yet.
