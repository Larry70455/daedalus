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

// Per frame performance log (compile time option DAEDALUS_PERF_LOG).
//
// What it does, in plain words:
//   The emulator's time is split into "buckets" (dynarec, display list,
//   texture conversion, GPU wait, and so on). Code that we want to measure
//   is wrapped in DAEDALUS_PERF_SCOPE(bucket). When a scope starts, the time
//   spent so far is charged to whatever bucket was running before it, and the
//   new bucket takes over. When the scope ends, the previous bucket takes over
//   again. So every microsecond is counted exactly once, in the innermost
//   bucket that was running ("exclusive" time).
//
//   At every N64 vertical blank (VI interrupt, 60 per second on NTSC) one line
//   is added to a CSV file named after the game:
//     <emulator folder>/PerfLogs/<Game Name>_<CRC>.csv
//   Lines are kept in memory and written in batches so the Memory Stick write
//   does not disturb the measurement much. The time spent writing is itself
//   reported in the "log_io" column. Two more columns count how many N64
//   instructions ran in the plain interpreter and how many were interpreter
//   fallbacks called from recompiled code.
//
//   A second file, <Game Name>_<CRC>_fallbacks.txt, lists which instructions
//   the dynarec hands back to the interpreter, sorted by how often they ran.
//   It is rewritten every 600 vertical blanks and when the game is closed.
//
// When DAEDALUS_PERF_LOG is not defined (the normal build) every macro below
// expands to nothing, so there is zero cost.

#ifndef UTILITY_PERFLOG_H_
#define UTILITY_PERFLOG_H_

#ifdef DAEDALUS_PERF_LOG

#include <vector>

namespace PerfLog
{
	// Keep in sync with kCategoryNames in PerfLog.cpp
	enum Category
	{
		kCpuOther = 0,	// Interpreter, CPU event handling, memory handlers: anything not listed below
		kDynarec,		// Running recompiled code (includes interpreter fallbacks called from inside it)
		kDisplayList,	// Parsing the N64 display list (excluding the two buckets below)
		kVertex,		// Vertex transform, lighting and clipping
		kTexture,		// Texture conversion into the PSP format (and making it visible to the GPU)
		kGpuWait,		// Waiting for the PSP GPU to finish (sceGuSync)
		kAudioHle,		// Audio list processing on the main CPU (or the Media Engine hand off)
		kAudioWait,		// Spinning because the audio output buffer is full
		kLimiter,		// Sleeping in the speed limiter (idle time, not a bottleneck)
		kCompile,		// Dynarec code generation (turning a recorded trace into native code)
		kLogIo,			// Writing this log file
		kNumCategories
	};

	void	Push( Category category );
	void	Pop();

	void	OpenGame();		// Call after a ROM has been loaded
	void	CloseGame();	// Call before the ROM is unloaded
	void	EndFrame();		// Call once per N64 vertical blank
	void	NoteFlip();		// Call when the game presents a new image

	// Interpreter fallback counting (see FallbackThunk in PerfLog.cpp).
	// gInterpretedOps counts ops run by the plain interpreter loop (code that
	// is not, or not yet, recompiled). gFallbackOps counts ops that recompiled
	// code could not translate and handed back to an interpreter function.
	// Dynarec event counters (reported in <game>_interp_hot.txt)
	enum Stat
	{
		kStatFlushICache = 0,	// whole cache dropped: the game used the CACHE instruction
		kStatFlushOverlap,		// whole cache dropped: a write overlapped compiled code
		kStatFlushSize,			// whole cache dropped: too many fragments
		kStatFlushHotMap,		// whole cache dropped: hot trace table full
		kStatTraceStarted,
		kStatTraceAborted,
		kStatFragmentCreated,
		kStatAudioUnderrun,
		kStatInvalChecks,		// osInvalICache calls that did not flush and were verified
		kStatStaleOps,			// compiled instructions that no longer match memory after such a call (must be 0)
		kStatPartialInvalidate,	// a write overlapped compiled code and only the affected fragments were dropped
		kStatFragmentsDropped,	// fragments dropped that way
		kNumStats
	};
	extern u32 gStats[ kNumStats ];
	void	NoteInterpretedPC( u32 pc );

	// Stale code verifier: every compiled instruction is recorded, and
	// CountStaleCompiledOps compares them all with current memory.
	void	NoteCompiledOp( u32 address, u32 op );
	void	ClearCompiledOps();
	void	ForgetCompiledOpsInPages( const std::vector< u16 > & pages );	// 4 KB pages numbered from 0x80000000
	u32		CountStaleCompiledOps();	// Counts one interpreted op and builds the hot address table
	extern u32 gInterpretedOps;
	extern u32 gFallbackOps;
	void	NoteFallbackSite( u32 op_code_bits );	// Called by the dynarec when it emits a fallback call
	void	FallbackThunk( u32 op_code_bits, void ( *handler )( u32 ) );	// Called from generated code

	struct Scope
	{
		explicit Scope( Category category )	{ Push( category ); }
		~Scope()							{ Pop(); }
		Scope( const Scope & ) = delete;
		Scope & operator=( const Scope & ) = delete;
	};
}

#define DAEDALUS_PERF_CONCAT_INNER( a, b )	a##b
#define DAEDALUS_PERF_CONCAT( a, b )		DAEDALUS_PERF_CONCAT_INNER( a, b )
#define DAEDALUS_PERF_SCOPE( category )		PerfLog::Scope DAEDALUS_PERF_CONCAT( perf_scope_, __LINE__ )( PerfLog::category )
#define DAEDALUS_PERF_OPEN_GAME()			PerfLog::OpenGame()
#define DAEDALUS_PERF_CLOSE_GAME()			PerfLog::CloseGame()
#define DAEDALUS_PERF_END_FRAME()			PerfLog::EndFrame()
#define DAEDALUS_PERF_NOTE_FLIP()			PerfLog::NoteFlip()
#define DAEDALUS_PERF_COUNT_INTERPRETED_OP( pc )	PerfLog::NoteInterpretedPC( pc )
#define DAEDALUS_PERF_STAT( s )	( PerfLog::gStats[ PerfLog::s ]++ )

#else

#define DAEDALUS_PERF_SCOPE( category )		do {} while( 0 )
#define DAEDALUS_PERF_OPEN_GAME()			do {} while( 0 )
#define DAEDALUS_PERF_CLOSE_GAME()			do {} while( 0 )
#define DAEDALUS_PERF_END_FRAME()			do {} while( 0 )
#define DAEDALUS_PERF_NOTE_FLIP()			do {} while( 0 )
#define DAEDALUS_PERF_COUNT_INTERPRETED_OP( pc )	do {} while( 0 )
#define DAEDALUS_PERF_STAT( s )	do {} while( 0 )

#endif // DAEDALUS_PERF_LOG

#endif // UTILITY_PERFLOG_H_
