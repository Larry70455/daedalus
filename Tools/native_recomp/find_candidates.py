#!/usr/bin/env python3
"""
Lists functions of a ROM that the native code prototype can run safely.

usage: find_candidates.py <rom .z64> <statecheck_os.txt> <segment rom offset> <segment vram> <segment size>

statecheck_os.txt comes from a StateCheck run with "oslist=1" (the OS
functions Daedalus found and patches). A function is listed when:
  - it calls at least one function, and every call goes to an OS function
    that Daedalus handles with HLE and that never switches threads
    (cache, memory copy, time and address helpers),
  - it has no FPU instructions and no indirect calls,
  - it is not patched by Daedalus itself.
Output lines: <vram> <size> <calls>, ready for a functions.txt (rom_to_elf.py).
Function bounds are guessed: a function starts at "addiu sp, sp, -N" and ends
at the last "jr ra" (plus delay slot) before the next such start.
"""
import struct, sys

NON_BLOCKING = { 'osInvalDCache', 'osWritebackDCache', 'osWritebackDCacheAll', 'osInvalICache',
                 'osVirtualToPhysical', 'bzero', 'bcopy', 'memcpy', 'osGetCount', 'osGetTime',
                 'strlen', 'strchr' }
FPU_OPS = { 0x11, 0x31, 0x35, 0x39, 0x3D }	# COP1, LWC1, LDC1, SWC1, SDC1

rom_path, os_path = sys.argv[1:3]
seg_rom, seg_vram, seg_size = (int(x, 16) for x in sys.argv[3:6])
rom = open(rom_path, 'rb').read()
patches = {}
for line in open(os_path):
    p = line.split()
    if len(p) == 2:
        patches[int(p[0], 16)] = p[1]

code = rom[seg_rom: seg_rom + seg_size]
words = struct.unpack('>%dI' % (len(code) // 4), code)
starts = [i for i, w in enumerate(words) if (w >> 16) == 0x27BD and (w & 0x8000)]
for k in range(len(starts) - 1):
    s, e = starts[k], starts[k + 1]
    body = words[s:e]
    ends = [j for j, w in enumerate(body) if w == 0x03E00008]
    if not ends:
        continue
    body = body[: ends[-1] + 2]
    vram = seg_vram + 4 * s
    if vram in patches:
        continue
    calls = [0x80000000 | ((w & 0x3FFFFFF) << 2) for w in body if w >> 26 == 3]
    if not calls or any(w >> 26 in FPU_OPS for w in body) or any(w >> 26 == 0 and (w & 0x3F) == 9 for w in body):
        continue
    names = [patches.get(t) for t in calls]
    if all(n in NON_BLOCKING for n in names):
        print(f'0x{vram:08X} 0x{len(body) * 4:X} ' + ' '.join(sorted({f'{n}=0x{t:08X}' for n, t in zip(names, calls)})))
