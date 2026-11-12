	.set noreorder

#include "pspstub.s"

	STUB_START "pspOverclock",0x40090000,0x00010005
	STUB_FUNC  0x4AE097C3,pspOverclockSet
	STUB_END
