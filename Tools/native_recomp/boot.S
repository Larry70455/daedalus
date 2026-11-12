# Minimal boot stub for the native code test ROM (no Nintendo IPL3 code).
# Daedalus starts the CPU at 0xA4000040 with the first 4 KB of the ROM copied
# to SP memory, so this runs from there. It copies the program (ROM offset
# 0x1000) to RDRAM 0x80000400 with a PI DMA and jumps to it. Real hardware
# would not boot this ROM (no CIC handshake); it is for emulators only.
	.set noreorder
	.set noat
	.section .text
	.globl _boot
_boot:
	lui   $t0, 0xA460           # PI registers
	li    $t1, 0x00000400       # PI_DRAM_ADDR
	sw    $t1, 0x00($t0)
	lui   $t1, 0x1000           # PI_CART_ADDR = 0x10001000
	ori   $t1, $t1, 0x1000
	sw    $t1, 0x04($t0)
	lui   $t1, %hi(PROGRAM_SIZE_MINUS_1)
	ori   $t1, $t1, %lo(PROGRAM_SIZE_MINUS_1)
	sw    $t1, 0x0C($t0)        # PI_WR_LEN: cartridge to RDRAM
1:	lw    $t2, 0x10($t0)        # PI_STATUS: wait while busy
	andi  $t2, $t2, 3
	bnez  $t2, 1b
	nop
	lui   $t0, 0x8000
	ori   $t0, $t0, 0x0400
	jr    $t0
	nop
