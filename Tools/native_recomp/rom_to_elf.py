#!/usr/bin/env python3
"""
Makes an ELF with symbols from functions in a commercial ROM, so N64Recomp
and gen_module.py can recompile them without a decompilation.

usage: rom_to_elf.py <rom .z64> <functions file> <out dir>

functions file, one per line (# comments allowed):
  segment <rom offset> <vram>          code segment mapping (ROM to RAM), once
  func <name> <vram> <size in bytes>   function to recompile
  extern <name> <vram>                 function it calls that stays in the emulator

Writes <out dir>/funcs.elf. The ROM bytes are copied as data words, so the
recompiled code is exactly the game's code; nothing from the ROM is stored in
the repository.
"""
import os, subprocess, sys

rom_path, list_path, out = sys.argv[1:4]
rom = open(rom_path, 'rb').read()
seg_rom = seg_vram = None
funcs, externs = [], []
for line in open(list_path):
    p = line.split('#')[0].split()
    if not p:
        continue
    if p[0] == 'segment':
        seg_rom, seg_vram = int(p[1], 16), int(p[2], 16)
    elif p[0] == 'func':
        funcs.append((p[1], int(p[2], 16), int(p[3], 16)))
    elif p[0] == 'extern':
        externs.append((p[1], int(p[2], 16)))

os.makedirs(out, exist_ok=True)
asm, ld = ['\t.set noreorder', '\t.set noat'], ['ENTRY(%s)' % funcs[0][0], 'SECTIONS', '{']
for name, vram, size in funcs:
    off = seg_rom + (vram - seg_vram)
    asm += [f'\t.section .text.{name}, "ax"', f'\t.globl {name}', f'\t.type {name}, @function', f'{name}:']
    for i in range(0, size, 4):
        asm.append('\t.word 0x' + rom[off + i: off + i + 4].hex())
    asm.append(f'\t.size {name}, . - {name}')
    ld.append(f'\t.text.{name} 0x{vram:08X} : {{ *(.text.{name}) }}')
for name, vram in externs:
    # A two instruction stand-in so the call target is a known function; it is never used
    asm += [f'\t.section .text.{name}, "ax"', f'\t.globl {name}', f'\t.type {name}, @function', f'{name}:',
            '\tjr $ra', '\tnop', f'\t.size {name}, . - {name}']
    ld.append(f'\t.text.{name} 0x{vram:08X} : {{ *(.text.{name}) }}')
ld += ['\t/DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.gnu.attributes) }', '}']
open(f'{out}/funcs.s', 'w').write('\n'.join(asm) + '\n')
open(f'{out}/funcs.ld', 'w').write('\n'.join(ld) + '\n')
subprocess.check_call(['mips-linux-gnu-as', '-EB', '-march=vr4300', '-mabi=32', f'{out}/funcs.s', '-o', f'{out}/funcs.o'])
subprocess.check_call(['mips-linux-gnu-ld', '-EB', '-T', f'{out}/funcs.ld', f'{out}/funcs.o', '-o', f'{out}/funcs.elf'])
print(f'{out}/funcs.elf: {len(funcs)} functions, {len(externs)} external calls')
