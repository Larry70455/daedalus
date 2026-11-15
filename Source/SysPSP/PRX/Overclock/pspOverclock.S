	.set noreorder

#include "pspstub.s"

	STUB_START "pspOverclock",0x40090000,0x00030005
	STUB_FUNC  0x4AE097C3,pspOverclockSet
	STUB_FUNC  0x6FDABE21,pspProfilerStart
	STUB_FUNC  0xEBB2261F,pspProfilerRead
	STUB_END
