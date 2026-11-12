/*
Copyright (C) 2026 DaedalusX64 Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// See NativeCode.h and NATIVE_CODE.md

#include "Base/Types.h"
#include "Native/NativeCode.h"
#include "Native/NativeModule.h"

#include "Core/CPU.h"
#include "Core/Interpret.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/R4300.h"
#include "Interface/ConfigOptions.h"
#include "OSHLE/patch.h"
#include "Ultra/ultra_R4300.h"

#ifdef DAEDALUS_ENABLE_DYNAREC
#include "DynaRec/Fragment.h"
#include "DynaRec/FragmentCache.h"
#endif

#include <cstdio>
#include <cstring>

bool			gNativeCodeEnabled = true;
NativeStats		gNativeStats;

extern "C" uint32_t	gNativePendingOps = 0;
extern "C" uint8_t *	gNativeRamBase = nullptr;

// All modules built into this executable (each generated file registers itself)
static const u32			kMaxNativeModules = 16;
static const NativeModule *	gNativeModules[ kMaxNativeModules ];
static u32					gNumNativeModules = 0;

extern "C" void native_register_module( const NativeModule * module )
{
	if( gNumNativeModules < kMaxNativeModules )
		gNativeModules[ gNumNativeModules++ ] = module;
}

static const NativeModule *	gActiveModule = nullptr;

//*****************************************************************************
// Module selection and insertion
//*****************************************************************************
void Native_RomOpened( u32 crc1, u32 crc2 )
{
	gActiveModule = nullptr;
	memset( &gNativeStats, 0, sizeof( gNativeStats ) );
	for( u32 i = 0; i < gNumNativeModules; ++i )
	{
		if( gNativeModules[ i ]->crc1 == crc1 && gNativeModules[ i ]->crc2 == crc2 )
		{
			gActiveModule = gNativeModules[ i ];
			printf( "Native code: module \"%s\" with %u functions for this ROM\n", gActiveModule->name, gActiveModule->num_funcs );
		}
	}
}

const char * Native_ModuleName()
{
	return gActiveModule ? gActiveModule->name : nullptr;
}

static u32 HashGuestCode( u32 vram, u32 num_ops )
{
	u32 phys = vram & 0x1FFFFFFF;
	if( phys + num_ops * 4 > gRamSize )
		return 0;
	const u32 * p = reinterpret_cast< const u32 * >( g_pu8RamBase + phys );
	u32 h = 2166136261u;
	for( u32 i = 0; i < num_ops; ++i )
		h = ( h ^ p[ i ] ) * 16777619u;
	return h;
}

bool Native_TryInsert( u32 pc )
{
#ifdef DAEDALUS_ENABLE_DYNAREC
	if( gActiveModule == nullptr || !gNativeCodeEnabled )
		return false;

	for( u32 i = 0; i < gActiveModule->num_funcs; ++i )
	{
		const NativeFunction & f = gActiveModule->funcs[ i ];
		if( f.vram != pc )
			continue;

		// Only while the guest code is the code this was compiled from: an
		// overlay or DMA that replaced it must run through the dynarec instead
		if( HashGuestCode( f.vram, f.num_ops ) != f.code_hash )
		{
			gNativeStats.HashMismatch++;
			return false;
		}

		// Same kind of fragment as the OS patches: it calls the trampoline and
		// then continues at gCPUState.TargetPC. The exit charges 1 cycle, the
		// rest is charged in native_execute.
		CFragment * fragment = new CFragment( gFragmentCache.GetCodeBufferManager(), f.vram, f.num_ops,
											  reinterpret_cast< void * >( f.trampoline ), 1 );
		gFragmentCache.InsertFragment( fragment );
		gNativeStats.Inserted++;
		return true;
	}
#endif
	return false;
}

//*****************************************************************************
// Register state transfer
//*****************************************************************************
static void LoadContext( recomp_context * ctx )
{
	gpr * r = &ctx->r0;
	for( u32 i = 0; i < 32; ++i )
		r[ i ] = gGPR[ i ]._u64;
	ctx->hi = gCPUState.MultHi._u64;
	ctx->lo = gCPUState.MultLo._u64;

	// FPU in 32 bit mode (SR.FR = 0): even/odd pairs share one 64 bit register
	fpr * f = &ctx->f0;
	for( u32 i = 0; i < 32; i += 2 )
	{
		f[ i ].u32l = gCPUState.FPU[ i ]._u32;
		f[ i ].u32h = gCPUState.FPU[ i + 1 ]._u32;
		f[ i + 1 ].u64 = 0;
	}
	ctx->status_reg = gCPUState.CPUControl[ C0_SR ]._u32;
	ctx->mips3_float_mode = ( ctx->status_reg & SR_FR ) != 0;
	ctx->f_odd = &ctx->f0.u32h;
}

static void StoreContext( const recomp_context * ctx )
{
	const gpr * r = &ctx->r0;
	for( u32 i = 1; i < 32; ++i )
		gGPR[ i ]._u64 = r[ i ];
	gGPR[ 0 ]._u64 = 0;
	gCPUState.MultHi._u64 = ctx->hi;
	gCPUState.MultLo._u64 = ctx->lo;

	const fpr * f = &ctx->f0;
	for( u32 i = 0; i < 32; i += 2 )
	{
		gCPUState.FPU[ i ]._u32 = f[ i ].u32l;
		gCPUState.FPU[ i + 1 ]._u32 = f[ i ].u32h;
	}
}

// Charge the guest instructions native code has run so far: same counter and
// events as the interpreter (an event that falls inside a native run is
// delivered when the native code returns, as with any compiled fragment)
static void FlushPendingOps( u32 keep )
{
	if( gNativePendingOps > keep )
	{
		u32 ops = gNativePendingOps - keep;
		gNativeStats.NativeOps += ops;
		CPU_UpdateCounter( ops );
	}
	gNativePendingOps = 0;
}

//*****************************************************************************
// Entry from a native fragment
//*****************************************************************************
extern "C" uint32_t native_execute( const NativeFunction * f )
{
	recomp_context ctx;
	LoadContext( &ctx );
	gNativeRamBase = g_pu8RamBase;
	gNativePendingOps = 0;
	gNativeStats.NativeCalls++;

	f->func( g_pu8RamBase, &ctx );

	StoreContext( &ctx );
	// The fragment exit charges one more instruction
	if( gNativePendingOps > 0 )
		gNativeStats.NativeOps++;
	FlushPendingOps( 1 );

	// Like an OS patch returning with "jr ra"
	gCPUState.TargetPC = (u32)ctx.r31;
	return 1;
}

//*****************************************************************************
// Calls from native code into guest code that is not native
//*****************************************************************************
#ifdef DAEDALUS_ENABLE_OS_HOOKS
extern PatchFunction Patch_GetFunctionAt( u32 vram );
extern void Patch_ForEachFunction( void ( *fn )( u32 vram, const char * name ) );
extern u32 gNumOfOSFunctions;
#endif

#ifdef DAEDALUS_ENABLE_OS_HOOKS
// Quick filter for "is there an OS patch at this address" (checked on every
// interpreted instruction): one bit per address hash, rebuilt when the set of
// patched functions changes
static u32	gPatchFilter[ 4096 / 32 ];
static u32	gPatchFilterCount = ~0u;

static PatchFunction FindPatch( u32 pc )
{
	if( gPatchFilterCount != gNumOfOSFunctions )
	{
		memset( gPatchFilter, 0, sizeof( gPatchFilter ) );
		Patch_ForEachFunction( []( u32 vram, const char * ) { u32 h = ( vram >> 2 ) & 4095; gPatchFilter[ h >> 5 ] |= 1u << ( h & 31 ); } );
		gPatchFilterCount = gNumOfOSFunctions;
	}
	u32 h = ( pc >> 2 ) & 4095;
	if( ( gPatchFilter[ h >> 5 ] & ( 1u << ( h & 31 ) ) ) == 0 )
		return nullptr;
	return Patch_GetFunctionAt( pc );
}
#endif

extern "C" uint32_t native_call_guest( uint32_t vram, uint8_t * rdram [[maybe_unused]], recomp_context * ctx )
{
	StoreContext( ctx );
	FlushPendingOps( 0 );
	gNativeStats.GuestCalls++;

	// The generated code set ra to the instruction after the jal, as the CPU
	// does. The call has returned when execution reaches that address again
	// with the caller's stack pointer (other threads and deeper recursion use
	// other stack pointers).
	const u32 return_pc = (u32)ctx->r31;
	const u32 caller_sp = (u32)ctx->r29;
	const u32 saved_pc = gCPUState.CurrentPC;
	const u32 saved_target = gCPUState.TargetPC;
	const u32 saved_delay = gCPUState.Delay;

	// Run the guest function the way the dynarec would, one instruction at a
	// time: whenever execution arrives at an OS function that Daedalus
	// patches (osRecvMesg, osSendMesg, osStartThread, osInvalDCache, ...), the
	// HLE patch runs instead of the guest code, exactly as with the dynarec's
	// patch fragments (same result, same 100 cycle charge, same thread
	// switches). Interrupts and exceptions are taken as usual. Thread switches
	// can run other threads here until this thread returns. Core changes and
	// stop requests stay queued until the native code has returned.
	gCPUState.CurrentPC = vram;
	gCPUState.Delay = NO_DELAY;
	while( !( gCPUState.CurrentPC == return_pc && gCPUState.Delay == NO_DELAY && (u32)gGPR[ 29 ]._u64 == caller_sp ) )
	{
#ifdef DAEDALUS_ENABLE_OS_HOOKS
		if( gCPUState.Delay == NO_DELAY && gOSHooksEnabled )
		{
			PatchFunction patch = FindPatch( gCPUState.CurrentPC );
			if( patch != nullptr )
			{
				gNativeStats.GuestCallsHle++;
				gCPUState.TargetPC = (u32)gGPR[ 31 ]._u64;
				u32 result = patch();
				// Continue where a patch fragment would: TargetPC, or after an
				// ERET style return the instruction after CurrentPC
				gCPUState.CurrentPC = result != 0 ? gCPUState.TargetPC : gCPUState.CurrentPC + 4;
				gCPUState.Delay = NO_DELAY;
				CPU_UpdateCounter( 100 );
			}
		}
		if( gCPUState.CurrentPC == return_pc && gCPUState.Delay == NO_DELAY && (u32)gGPR[ 29 ]._u64 == caller_sp )
			break;
#endif
		u32 stuff = gCPUState.GetStuffToDo();
		if( stuff & CPU_CHECK_INTERRUPTS )
		{
			R4300_Handle_Interrupt();
			gCPUState.ClearJob( CPU_CHECK_INTERRUPTS );
			continue;
		}
		if( stuff & CPU_CHECK_EXCEPTIONS )
		{
			R4300_Handle_Exception();
			gCPUState.ClearJob( CPU_CHECK_EXCEPTIONS );
			continue;
		}
		Inter_ExecuteOneOp();
		gNativeStats.GuestOps++;
	}

	gCPUState.CurrentPC = saved_pc;
	gCPUState.TargetPC = saved_target;
	gCPUState.Delay = saved_delay;
	LoadContext( ctx );
	return 0;
}

// Indirect calls from native code (jalr, function pointers)
static u32 gLookupTarget = 0;
static void LookupThunk( uint8_t * rdram, recomp_context * ctx )
{
	native_call_guest( gLookupTarget, rdram, ctx );
}

extern "C" recomp_func_t * native_lookup_func( int32_t vram )
{
	gLookupTarget = (u32)vram;
	return LookupThunk;
}

//*****************************************************************************
// Memory access outside RDRAM, and the rest of the N64Recomp runtime API
//*****************************************************************************
extern "C" uint32_t native_slow_read32( uint32_t address )	{ return Read32Bits( address ); }
extern "C" uint32_t native_slow_read16( uint32_t address )	{ return Read16Bits( address ); }
extern "C" uint32_t native_slow_read8( uint32_t address )	{ return Read8Bits( address ); }
extern "C" void native_slow_write32( uint32_t address, uint32_t value )	{ Write32Bits( address, value ); }
extern "C" void native_slow_write16( uint32_t address, uint32_t value )	{ Write16Bits( address, (u16)value ); }
extern "C" void native_slow_write8( uint32_t address, uint32_t value )	{ Write8Bits( address, (u8)value ); }

extern "C" void cop0_status_write( recomp_context * ctx, gpr value )
{
	ctx->status_reg = (uint32_t)value;
	R4300_SetSR( (u32)value );
}

extern "C" gpr cop0_status_read( recomp_context * ctx [[maybe_unused]] )
{
	return (gpr)(s32)gCPUState.CPUControl[ C0_SR ]._u32;
}

extern "C" void switch_error( const char * func, uint32_t vram, uint32_t jtbl )
{
	printf( "Native code: bad jump table in %s at %08x (table %08x)\n", func, vram, jtbl );
}

extern "C" void do_break( uint32_t vram )
{
	printf( "Native code: break at %08x\n", vram );
}

extern "C" void recomp_syscall_handler( uint8_t * rdram [[maybe_unused]], recomp_context * ctx [[maybe_unused]], int32_t vram )
{
	printf( "Native code: syscall at %08x is not supported\n", vram );
}

extern "C" void pause_self( uint8_t * rdram [[maybe_unused]] ) {}

extern "C" { int32_t * section_addresses = nullptr; }
