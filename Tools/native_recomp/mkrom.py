#!/usr/bin/env python3
"""Build the native code test ROM: header + boot stub + program, big endian (.z64)."""
import struct, sys

boot, prog, out = sys.argv[1], sys.argv[2], sys.argv[3]
boot_bin = open(boot, 'rb').read()
prog_bin = open(prog, 'rb').read()
assert len(boot_bin) <= 0x1000 - 0x40, "boot stub too large"

header = bytearray(0x40)
struct.pack_into('>IIII', header, 0, 0x80371240, 0x0000000F, 0x80000400, 0x0000144B)
struct.pack_into('>II', header, 0x10, 0x4E415456, 0x54455354)   # CRC1/CRC2 used as the ROM ID ("NATV", "TEST")
header[0x20:0x34] = b'DAEDALUS NATIVE TEST'.ljust(20)
header[0x3B:0x3F] = b'NNTE'     # media N, id "NT", country E
header[0x3F] = 0

rom = bytearray(header) + boot_bin.ljust(0x1000 - 0x40, b'\0') + prog_bin
rom = rom.ljust(max(len(rom), 1 << 20), b'\0')
open(out, 'wb').write(rom)
print(f"{out}: {len(rom)} bytes, program {len(prog_bin)} bytes")
