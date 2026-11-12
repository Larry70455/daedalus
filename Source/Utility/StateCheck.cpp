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

// See StateCheck.h for an overview. This whole file compiles to nothing
// unless DAEDALUS_STATE_CHECK is defined.

#include "Base/Types.h"
#include "Utility/StateCheck.h"

#ifdef DAEDALUS_STATE_CHECK

#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Ultra/ultra_R4300.h"	// C0_COUNT
#include "Interface/ConfigOptions.h"
#include "Interface/SaveState.h"
#include "System/SystemInit.h"
#include "Utility/Paths.h"
#include "Utility/PerfLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#ifdef DAEDALUS_PSP
#include <pspkernel.h>
#endif

static FILE *	gOut = nullptr;
static u32		gFramesWanted = 0;
static u32		gFrame = 0;
static int		gAudioOverride = -1;
static int		gMemOptOverride = -1;
static int		gPatchesOverride = -1;	// patches=0|1 overrides the OS function patches (HLE) ROM setting	// memopt=0|1 overrides the "memory access optimisation" ROM setting	// audio=0|1|2 in statecheck.txt (off, async, sync); -1 keeps the ROM setting
static bool		gHashRam = true;	// ramhash=0 in statecheck.txt turns the RAM checksum off (for timing runs)

// FNV-1a over 32 bit words. Not cryptographic, only needs to change when the
// data changes.
static u32 HashWords( const void * data, u32 bytes )
{
	const u32 * p = static_cast< const u32 * >( data );
	u32 h = 2166136261u;
	for( u32 i = 0; i < bytes / 4; ++i )
	{
		h ^= p[ i ];
		h *= 16777619u;
	}
	return h;
}

static void Finish()
{
	if( gOut != nullptr )
	{
		fprintf( gOut, "# done after %u vertical blanks\n", gFrame );
		fclose( gOut );
		gOut = nullptr;
	}
	printf( "StateCheck: finished %u frames\n", gFrame );

	// The program exits right below without the normal shutdown, so write out
	// the perf log first if this build has one (no-op otherwise).
	DAEDALUS_PERF_CLOSE_GAME();

#ifdef DAEDALUS_PSP
	sceKernelExitGame();
#else
	std::exit( 0 );
#endif
}

bool StateCheck_Start()
{
	std::ifstream cfg( setBasePath( "statecheck.txt" ) );
	if( !cfg.is_open() )
		return false;

	std::string rom, mode = "interpreter", savestate;
	u32 frames = 300;

	std::string line;
	while( std::getline( cfg, line ) )
	{
		if( !line.empty() && line.back() == '\r' )
			line.pop_back();
		if( line.empty() || line[ 0 ] == '#' )
			continue;
		size_t eq = line.find( '=' );
		if( eq == std::string::npos )
			continue;
		std::string key = line.substr( 0, eq );
		std::string value = line.substr( eq + 1 );
		if( key == "rom" )				rom = value;
		else if( key == "mode" )		mode = value;
		else if( key == "frames" )		frames = (u32)std::strtoul( value.c_str(), nullptr, 10 );
		else if( key == "savestate" )	savestate = value;
		else if( key == "patches" )		gPatchesOverride = std::atoi( value.c_str() );
		else if( key == "memopt" )		gMemOptOverride = std::atoi( value.c_str() );
		else if( key == "audio" )		gAudioOverride = std::atoi( value.c_str() );
		else if( key == "ramhash" )		gHashRam = ( value != "0" );
	}

	if( rom.empty() || frames == 0 || ( mode != "interpreter" && mode != "dynarec" ) )
	{
		printf( "StateCheck: statecheck.txt needs rom=, frames= and mode=interpreter|dynarec\n" );
		return false;
	}

	printf( "StateCheck: rom %s, mode %s, %u frames\n", rom.c_str(), mode.c_str(), frames );

	if( !System_Open( setBasePath( rom ) ) )
	{
		printf( "StateCheck: could not open %s\n", rom.c_str() );
		return false;
	}

	if( !savestate.empty() && !SaveState_LoadFromFile( setBasePath( savestate ) ) )
	{
		printf( "StateCheck: could not load savestate %s\n", savestate.c_str() );
		return false;
	}

	// Override the per ROM preferences that System_Open applied
	gDynarecEnabled = ( mode == "dynarec" );
	if( gPatchesOverride >= 0 )
		gOSHooksEnabled = ( gPatchesOverride != 0 );
	if( gMemOptOverride >= 0 )
		gMemoryAccessOptimisation = ( gMemOptOverride != 0 );
	if( gAudioOverride >= 0 && gAudioOverride <= 2 )
		gAudioPluginEnabled = static_cast< EAudioPluginMode >( gAudioOverride );
	gSpeedSyncEnabled = 0;	// No speed limiting: the result must not depend on wall clock time

	std::string out_name = "statecheck_" + mode + ".txt";
	gOut = fopen( setBasePath( out_name ).string().c_str(), "w" );
	if( gOut == nullptr )
	{
		printf( "StateCheck: could not create %s\n", out_name.c_str() );
		return false;
	}

	fprintf( gOut, "# statecheck rom=%s mode=%s frames=%u savestate=%s\n", rom.c_str(), mode.c_str(), frames, savestate.c_str() );
	fprintf( gOut, "# vi pc count gpr_hash fpr_hash fcr31 hi lo rdram_hash | r0..r31\n" );

	gFramesWanted = frames;
	gFrame = 0;
	return true;
}

void StateCheck_OnVerticalBlank()
{
	if( gOut == nullptr )
		return;

	const SCPUState & s = gCPUState;

	fprintf( gOut, "%u %08x %08x %08x %08x %08x %016llx %016llx %08x |",
			 gFrame,
			 s.CurrentPC,
			 s.CPUControl[ C0_COUNT ]._u32,
			 HashWords( &s.CPU[ 0 ], sizeof( s.CPU ) ),
			 HashWords( &s.FPU[ 0 ], sizeof( s.FPU ) ),
			 s.FPUControl[ 31 ]._u32,
			 (unsigned long long)s.MultHi._u64,
			 (unsigned long long)s.MultLo._u64,
			 gHashRam ? HashWords( g_pu8RamBase, gRamSize ) : 0 );

	for( u32 i = 0; i < 32; ++i )
		fprintf( gOut, " %016llx", (unsigned long long)s.CPU[ i ]._u64 );
	fprintf( gOut, "\n" );

	gFrame++;
	if( gFrame >= gFramesWanted )
		Finish();
}

#endif // DAEDALUS_STATE_CHECK
