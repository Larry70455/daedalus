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

// See PerfLog.h for an overview. This whole file compiles to nothing unless
// DAEDALUS_PERF_LOG is defined.

#include "Base/Types.h"
#include "Utility/PerfLog.h"

#ifdef DAEDALUS_PERF_LOG

#include "Core/ROM.h"
#include "Utility/Paths.h"

#include <cstdio>
#include <string>
#include <filesystem>

#ifdef DAEDALUS_PSP
#include <pspthreadman.h>
#else
#include <chrono>
#endif

namespace PerfLog
{

static const char * const kCategoryNames[ kNumCategories ] =
{
	"cpu_other", "dynarec", "dlist", "vertex", "texture",
	"gpu_wait", "audio_hle", "audio_wait", "limiter", "log_io",
};

// Microsecond clock. On the PSP this is the kernel system timer, which is a
// cheap call with 1 us resolution. Only differences are used, so the 32 bit
// wrap around (every ~71 minutes) is harmless.
static inline u32 NowMicros()
{
#ifdef DAEDALUS_PSP
	return sceKernelGetSystemTimeLow();
#else
	using namespace std::chrono;
	return (u32)duration_cast< microseconds >( steady_clock::now().time_since_epoch() ).count();
#endif
}

static const u32	kMaxDepth = 16;			// Scopes nest only a few levels deep in practice
static const size_t	kFlushThreshold = 24 * 1024;	// Bytes of text kept in memory before writing

static bool			gActive = false;
static FILE *		gFile = nullptr;
static std::string	gPending;

static Category		gStack[ kMaxDepth ];
static u32			gDepth = 0;				// Number of valid entries in gStack
static u32			gOverflow = 0;			// Pushes ignored because the stack was full
static u32			gLastMark = 0;			// Time at which the current bucket started being charged

static u32			gFrameMicros[ kNumCategories ];
static u32			gFrameStart = 0;
static u32			gFrameIndex = 0;
static u32			gFlipsThisFrame = 0;

// Totals for the summary written when the game is closed
static u64			gTotalMicros[ kNumCategories ];
static u64			gTotalFrameMicros = 0;
static u32			gTotalFlips = 0;

static inline Category Current()
{
	return gDepth > 0 ? gStack[ gDepth - 1 ] : kCpuOther;
}

// Charge the time since the last mark to the bucket that is currently running.
static inline void Charge( u32 now )
{
	gFrameMicros[ Current() ] += now - gLastMark;
	gLastMark = now;
}

void Push( Category category )
{
	if( !gActive )
		return;

	Charge( NowMicros() );

	if( gDepth < kMaxDepth )
		gStack[ gDepth++ ] = category;
	else
		gOverflow++;
}

void Pop()
{
	if( !gActive )
		return;

	Charge( NowMicros() );

	if( gOverflow > 0 )
		gOverflow--;
	else if( gDepth > 0 )
		gDepth--;
}

static void Flush()
{
	if( gFile == nullptr || gPending.empty() )
		return;

	// The write itself is timed and reported in the log_io column of the next line
	Push( kLogIo );
	fwrite( gPending.data(), 1, gPending.size(), gFile );
	fflush( gFile );
	gPending.clear();
	Pop();
}

static void ResetFrame( u32 now )
{
	for( u32 i = 0; i < kNumCategories; ++i )
		gFrameMicros[ i ] = 0;

	gFrameStart = now;
	gLastMark = now;
	gFlipsThisFrame = 0;
}

static std::string SafeFileName( const std::string & name )
{
	std::string out;
	for( char c : name )
	{
		bool ok = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '-' || c == '_';
		out += ok ? c : '_';
	}
	if( out.empty() )
		out = "Unknown";
	return out;
}

void OpenGame()
{
	CloseGame();

	std::filesystem::path dir = setBasePath( "PerfLogs" );
	std::error_code ec;
	std::filesystem::create_directories( dir, ec );

	char crc[ 24 ];
	snprintf( crc, sizeof( crc ), "_%08x", g_ROM.mRomID.CRC[ 0 ] );
	std::filesystem::path file = dir / ( SafeFileName( g_ROM.settings.GameName ) + crc + ".csv" );

	// Append, so several play sessions of the same game end up in one file.
	gFile = fopen( file.string().c_str(), "a" );
	if( gFile == nullptr )
		return;

	gPending.reserve( kFlushThreshold + 1024 );
	gPending.clear();

	char line[ 256 ];
	snprintf( line, sizeof( line ), "# session start, game \"%s\", crc %08x-%08x, units: microseconds per N64 vertical blank\n",
			  g_ROM.settings.GameName.c_str(), g_ROM.mRomID.CRC[ 0 ], g_ROM.mRomID.CRC[ 1 ] );
	gPending += line;

	gPending += "vi,flips,total";
	for( u32 i = 0; i < kNumCategories; ++i )
	{
		gPending += ',';
		gPending += kCategoryNames[ i ];
	}
	gPending += '\n';

	for( u32 i = 0; i < kNumCategories; ++i )
		gTotalMicros[ i ] = 0;
	gTotalFrameMicros = 0;
	gTotalFlips = 0;
	gFrameIndex = 0;
	gDepth = 0;
	gOverflow = 0;

	gActive = true;
	ResetFrame( NowMicros() );
}

void NoteFlip()
{
	if( gActive )
		gFlipsThisFrame++;
}

void EndFrame()
{
	if( !gActive )
		return;

	u32 now = NowMicros();
	Charge( now );

	u32 total = now - gFrameStart;

	char line[ 256 ];
	int len = snprintf( line, sizeof( line ), "%u,%u,%u", gFrameIndex, gFlipsThisFrame, total );
	for( u32 i = 0; i < kNumCategories && len > 0 && len < (int)sizeof( line ); ++i )
	{
		len += snprintf( line + len, sizeof( line ) - len, ",%u", gFrameMicros[ i ] );
		gTotalMicros[ i ] += gFrameMicros[ i ];
	}
	gPending += line;
	gPending += '\n';

	gTotalFrameMicros += total;
	gTotalFlips += gFlipsThisFrame;
	gFrameIndex++;

	ResetFrame( now );

	// Write after resetting, so the write lands in the next line's log_io column
	if( gPending.size() >= kFlushThreshold )
		Flush();
}

void CloseGame()
{
	if( !gActive )
		return;

	// Summary: average microseconds per vertical blank and share of the total
	if( gFrameIndex > 0 && gTotalFrameMicros > 0 )
	{
		char line[ 256 ];
		f32 seconds = f32( gTotalFrameMicros ) / 1000000.0f;
		snprintf( line, sizeof( line ), "# summary: %u vertical blanks, %u flips, %.1f seconds, %.1f flips per second\n",
				  gFrameIndex, gTotalFlips, seconds, seconds > 0.0f ? f32( gTotalFlips ) / seconds : 0.0f );
		gPending += line;

		for( u32 i = 0; i < kNumCategories; ++i )
		{
			snprintf( line, sizeof( line ), "# summary: %-10s avg %7.0f us  %5.1f%%\n", kCategoryNames[ i ],
					  f64( gTotalMicros[ i ] ) / f64( gFrameIndex ),
					  100.0 * f64( gTotalMicros[ i ] ) / f64( gTotalFrameMicros ) );
			gPending += line;
		}
	}

	Flush();
	gActive = false;

	if( gFile != nullptr )
	{
		fclose( gFile );
		gFile = nullptr;
	}
	gPending.clear();
	gPending.shrink_to_fit();
}

} // namespace PerfLog

#endif // DAEDALUS_PERF_LOG
