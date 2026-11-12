/*
 * CPU overclock above 333 MHz for DaedalusX64 (kernel module).
 *
 * The PLL programming below is adapted from psp-beyond-444mhz
 * (https://github.com/mcidclan/psp-beyond-444mhz, "overclock-picker"),
 * Copyright (c) 2026 m-c/d, MIT licence:
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to permit
 *   persons to whom the Software is furnished to do so, subject to the
 *   following conditions: The above copyright notice and this permission
 *   notice shall be included in all copies or substantial portions of the
 *   Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 *
 * How it works, in plain words:
 *   The PSP clock comes from a PLL: 37 MHz times a multiplier divided by a
 *   denominator. Sony's firmware stops at 333 MHz. This module first asks
 *   for 333 MHz the normal way, then raises the PLL multiplier in small steps
 *   (about 4 MHz each, with a pause after every step) until the target is
 *   reached. Going back down uses the same small steps. The system still
 *   reports 333 MHz afterwards; the real speed shows in the perf log CPU test.
 *
 * This is unofficial. An unstable frequency freezes the console until it is
 * rebooted. Not every unit reaches 400 MHz.
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>

PSP_MODULE_INFO("pspOverclock_Module", 0x1006, 1, 0);

#define hw(addr) (*((volatile unsigned int *)(addr)))
#define sync() asm volatile("sync")

#define PLL_BASE_MHZ	37
#define PLL_DEN			0x12	/* the denominator the picker uses by default */
#define MUL_333			162		/* 37 * 162 / 18 = 333 MHz */
#define MUL_MAX			254		/* about 522 MHz, never requested here */
#define STEP_DELAY_US	100000	/* pause after every step, as the picker does */

static inline void delayPipeline( void )
{
	asm volatile( "nop; nop; nop; nop; nop; nop; nop" );
}

/* Wait for clocks to settle (same loop length as the picker) */
static inline void settle( void )
{
	sync();
	volatile unsigned int i = 0x2ffff;
	while( --i )
		delayPipeline();
}

static inline int suspendIntr( void )
{
	return sceKernelCpuSuspendIntr();
}

static inline void resumeIntr( int intr )
{
	sceKernelCpuResumeIntr( intr );
}

static void unlockMemory( void )
{
	unsigned int reg;
	for( reg = 0xbc000000; reg <= 0xbc00002c; reg += 4 )
		hw( reg ) = -1;
	sync();
}

/* Raise the CPU and bus clock domain ratios to 1:1 in small steps */
static void adjustDomainRatios( void )
{
	int state = sceKernelSuspendDispatchThread();
	int intr = suspendIntr();

	const unsigned int cpu = hw( 0xbc200000 );
	const unsigned int bus = hw( 0xbc200004 );
	sync();

	unsigned int cpuDen = cpu & 0x1ff;
	unsigned int cpuNum = ( cpu >> 16 ) & 0x1ff;
	unsigned int busDen = bus & 0x1ff;
	unsigned int busNum = ( bus >> 16 ) & 0x1ff;

	const unsigned int step = 18;
	while( ( cpuNum & cpuDen & busNum & busDen ) != 0x1ff )
	{
		cpuNum = ( cpuNum + step > 0x1ff ) ? 0x1ff : cpuNum + step;
		cpuDen = ( cpuDen + step > 0x1ff ) ? 0x1ff : cpuDen + step;
		busNum = ( busNum + step > 0x1ff ) ? 0x1ff : busNum + step;
		busDen = ( busDen + step > 0x1ff ) ? 0x1ff : busDen + step;
		hw( 0xbc200000 ) = ( cpuNum << 16 ) | cpuDen;
		hw( 0xbc200004 ) = ( busNum << 16 ) | busDen;
		settle();
	}

	resumeIntr( intr );
	sceKernelResumeDispatchThread( state );
}

static void setMultiplier( unsigned int mul )
{
	int state = sceKernelSuspendDispatchThread();
	int intr = suspendIntr();

	hw( 0xbc100068 ) = 0x85;
	sync();
	do { delayPipeline(); } while( hw( 0xbc100068 ) & 0x80 );
	sync();
	settle();

	hw( 0xbc1000fc ) = ( hw( 0xbc1000fc ) & 0xffff0000 ) | ( mul << 8 ) | PLL_DEN;
	sync();
	settle();

	resumeIntr( intr );
	sceKernelResumeDispatchThread( state );
}

static unsigned int gCurrentMul = MUL_333;
static int gPrepared = 0;

/*
 * Set the CPU clock to about target_mhz (333 or less means back to normal).
 * Returns the frequency set, in MHz.
 */
int pspOverclockSet( int target_mhz )
{
	int k1 = pspSdkSetK1( 0 );

	unsigned int target = MUL_333;
	if( target_mhz > 333 )
	{
		target = ( (unsigned int)target_mhz * PLL_DEN + PLL_BASE_MHZ / 2 ) / PLL_BASE_MHZ;
		target &= ~1u;	/* the picker only uses even multipliers */
		if( target > MUL_MAX )
			target = MUL_MAX;
	}

	if( !gPrepared )
	{
		sceKernelIcacheInvalidateAll();
		unlockMemory();
		scePowerSetClockFrequency( 333, 333, 166 );
		adjustDomainRatios();
		/* Like the picker: if the PLL is not already at one of our steps,
		   treat it as 333 MHz (the first step rewrites it as 162 / 0x12) */
		const unsigned int pll = hw( 0xbc1000fc );
		sync();
		gCurrentMul = ( pll & 0xff00 ) >> 8;
		if( ( pll & 0xff ) != PLL_DEN || gCurrentMul < MUL_333 || gCurrentMul > MUL_MAX || ( gCurrentMul & 1 ) )
			gCurrentMul = MUL_333;
		gPrepared = 1;
	}

	while( gCurrentMul != target )
	{
		if( gCurrentMul < target )
			gCurrentMul += 2;
		else
			gCurrentMul -= 2;
		setMultiplier( gCurrentMul );
		sceKernelDelayThread( STEP_DELAY_US );
	}

	pspSdkSetK1( k1 );
	return (int)( gCurrentMul * PLL_BASE_MHZ / PLL_DEN );
}

int module_start( SceSize args, void * argp )
{
	return 0;
}

int module_stop( SceSize args, void * argp )
{
	return 0;
}
