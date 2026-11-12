ENTRY(_start)
SECTIONS
{
	. = 0x80000400;
	.text : { *(.text.start) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) *(.sdata*) }
	.bss : { *(.bss*) *(.sbss*) *(COMMON) }
	/DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug*) }
}
