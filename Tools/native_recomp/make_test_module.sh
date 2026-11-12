#!/bin/bash
# Builds the native code test ROM and regenerates its native module:
#   1. test ROM (big endian MIPS gcc)           -> <out>/native_test.z64, .elf
#   2. N64Recomp (path in $N64RECOMP)            -> <out>/recomp_out/native_test.c
#   3. gen_module.py                             -> Source/Native/Generated/native_test.c
# usage: N64RECOMP=/path/to/N64Recomp make_test_module.sh <out dir>
set -e
D=$(cd "$(dirname "$0")" && pwd); O=${1:?out dir}
"$D/make_rom.sh" "$O"
cp "$D/recomp.toml" "$O/"
( cd "$O" && rm -rf recomp_out && "${N64RECOMP:?set N64RECOMP to the N64Recomp executable}" recomp.toml )
python3 "$D/gen_module.py" native_test "$O/recomp_out/native_test.c" "$O/native_test.elf" "$O/native_test.z64" \
	"$D/../../Source/Native/Generated/native_test.c" native_checksum
