#!/bin/bash
# Builds native_test.z64 and native_test.elf with a big endian MIPS gcc
# (Ubuntu: apt install gcc-mips-linux-gnu). Usage: make_rom.sh <out dir>
set -e
D=$(cd "$(dirname "$0")" && pwd); O=${1:-$D/out}; mkdir -p "$O"
CC=${CC:-mips-linux-gnu-gcc}
CFLAGS="-march=vr4300 -mtune=vr4300 -mabi=32 -EB -mno-abicalls -fno-pic -G0 -O2 -ffreestanding -fno-builtin -nostdlib -mno-check-zero-division -mfix4300 -fno-toplevel-reorder"
$CC $CFLAGS -c "$D/crt0.S" -o "$O/crt0.o"
$CC $CFLAGS -c "$D/test.c" -o "$O/test.o"
mips-linux-gnu-ld -EB -T "$D/link.ld" -nostdlib "$O/crt0.o" "$O/test.o" -o "$O/native_test.elf"
mips-linux-gnu-objcopy -O binary "$O/native_test.elf" "$O/prog.bin"
SIZE=$(stat -c %s "$O/prog.bin")
$CC $CFLAGS -Wa,--defsym,PROGRAM_SIZE_MINUS_1=$((SIZE + 0x1000 - 1)) -c "$D/boot.S" -o "$O/boot.o"
mips-linux-gnu-objcopy -O binary -j .text "$O/boot.o" "$O/boot.bin"
python3 "$D/mkrom.py" "$O/boot.bin" "$O/prog.bin" "$O/native_test.z64"
