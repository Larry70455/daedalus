	.set noreorder
	.section .text.start
	.globl _start
	.type _start, @function
_start:
	lui   $sp, 0x8030           # stack below 3 MB
	jal   main
	nop
1:	j     1b
	nop
	.size _start, . - _start
