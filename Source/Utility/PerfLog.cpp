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
#include "Core/R4300OpCode.h"
#include "Utility/Paths.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <vector>

#ifdef DAEDALUS_PSP
#include <pspkernel.h>
#include <psppower.h>
#include <pspthreadman.h>
#else
#include <chrono>
#endif

#ifdef DAEDALUS_PSP
extern int gPspModel;
extern bool PSP_IS_SLIM;
#endif

namespace PerfLog
{

static const char * const kCategoryNames[ kNumCategories ] =
{
	"cpu_other", "dynarec", "dlist", "vertex", "texture",
	"gpu_wait", "audio_hle", "audio_wait", "limiter", "compile", "log_io",
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

u32					gInterpretedOps = 0;
u32					gStats[ kNumStats ];
u32					gFallbackOps = 0;

// Fallback statistics, keyed by the instruction "kind" (see FallbackKey)
struct FallbackStats
{
	u32		Count = 0;		// How many times it ran from recompiled code
	u32		Sites = 0;		// How many places in recompiled code call it
	u32		Example = 0;	// One full opcode of this kind, used to print its name
};
static std::unordered_map< u32, FallbackStats >	gFallbacks;
static std::filesystem::path	gFallbackPath;
static const u32				kFallbackDumpInterval = 600;	// Vertical blanks (about 10 seconds)

// Hot address table for the plain interpreter: how often each instruction
// address was interpreted (not recompiled). Open addressing, power of two size.
static const u32				kPcTableSize = 1 << 16;
static u32						gPcKey[ kPcTableSize ];
static u32						gPcCount[ kPcTableSize ];
static u32						gPcUsed = 0;

// Totals for the summary written when the game is closed
static u64			gTotalMicros[ kNumCategories ];
static u64			gTotalFrameMicros = 0;
static u32			gTotalFlips = 0;
static u64			gTotalInterpreted = 0;
static u64			gTotalFallback = 0;

// Reduce an opcode to its "kind" by dropping register numbers and immediates,
// so all ADD.D instructions are counted together no matter which registers
// they use. Kept fields: major opcode, plus the sub opcode fields that select
// the instruction for SPECIAL, REGIMM and the coprocessor groups.
static u32 FallbackKey( u32 op_bits )
{
	OpCode op;
	op._u32 = op_bits;
	u32 key = op.op << 16;
	switch( op.op )
	{
	case OP_SPECOP:	key |= op.spec_op; break;
	case OP_REGIMM:	key |= op.regimm_op; break;
	case OP_COPRO0:
	case OP_COPRO1:	key |= ( op.rs << 6 ) | ( op_bits & 0x3f ); break;	// fmt or sub op, plus function field
	default:		break;
	}
	return key;
}

// Name of the instruction kind, for example "ADD.D". The emulator's own
// disassembler is compiled out of release builds, so a compact table is kept
// here. Unknown encodings print as their hex fields.
static std::string FallbackName( u32 op_bits )
{
	static const char * const kMain[ 64 ] =
	{
		"SPECIAL","REGIMM","J","JAL","BEQ","BNE","BLEZ","BGTZ",
		"ADDI","ADDIU","SLTI","SLTIU","ANDI","ORI","XORI","LUI",
		"COP0","COP1","COP2","?","BEQL","BNEL","BLEZL","BGTZL",
		"DADDI","DADDIU","LDL","LDR","?","?","?","?",
		"LB","LH","LWL","LW","LBU","LHU","LWR","LWU",
		"SB","SH","SWL","SW","SDL","SDR","SWR","CACHE",
		"LL","LWC1","LWC2","?","LLD","LDC1","LDC2","LD",
		"SC","SWC1","SWC2","?","SCD","SDC1","SDC2","SD",
	};
	static const char * const kSpecial[ 64 ] =
	{
		"SLL","?","SRL","SRA","SLLV","?","SRLV","SRAV",
		"JR","JALR","?","?","SYSCALL","BREAK","?","SYNC",
		"MFHI","MTHI","MFLO","MTLO","DSLLV","?","DSRLV","DSRAV",
		"MULT","MULTU","DIV","DIVU","DMULT","DMULTU","DDIV","DDIVU",
		"ADD","ADDU","SUB","SUBU","AND","OR","XOR","NOR",
		"?","?","SLT","SLTU","DADD","DADDU","DSUB","DSUBU",
		"TGE","TGEU","TLT","TLTU","TEQ","?","TNE","?",
		"DSLL","?","DSRL","DSRA","DSLL32","?","DSRL32","DSRA32",
	};
	static const char * const kRegImm[ 32 ] =
	{
		"BLTZ","BGEZ","BLTZL","BGEZL","?","?","?","?",
		"TGEI","TGEIU","TLTI","TLTIU","TEQI","?","TNEI","?",
		"BLTZAL","BGEZAL","BLTZALL","BGEZALL","?","?","?","?",
		"?","?","?","?","?","?","?","?",
	};
	static const char * const kCopMove[ 16 ] =
	{
		"MFC","DMFC","CFC","?","MTC","DMTC","CTC","?",
		"BC","?","?","?","?","?","?","?",
	};
	static const char * const kCop1Funct[ 64 ] =
	{
		"ADD","SUB","MUL","DIV","SQRT","ABS","MOV","NEG",
		"ROUND.L","TRUNC.L","CEIL.L","FLOOR.L","ROUND.W","TRUNC.W","CEIL.W","FLOOR.W",
		"?","?","?","?","?","?","?","?",
		"?","?","?","?","?","?","?","?",
		"CVT.S","CVT.D","?","?","CVT.W","CVT.L","?","?",
		"?","?","?","?","?","?","?","?",
		"C.F","C.UN","C.EQ","C.UEQ","C.OLT","C.ULT","C.OLE","C.ULE",
		"C.SF","C.NGLE","C.SEQ","C.NGL","C.LT","C.NGE","C.LE","C.NGT",
	};

	OpCode op;
	op._u32 = op_bits;
	const u32 funct = op_bits & 0x3f;
	char buf[ 48 ];

	switch( op.op )
	{
	case OP_SPECOP:	return kSpecial[ funct ];
	case OP_REGIMM:	return kRegImm[ op.regimm_op ];
	case OP_COPRO0:
	case OP_COPRO1:
		{
			const char * cop = op.op == OP_COPRO0 ? "0" : "1";
			if( op.rs < 16 )
				return std::string( kCopMove[ op.rs ] ) + cop;
			if( op.op == OP_COPRO0 )
			{
				snprintf( buf, sizeof( buf ), "COP0.fn%02x", funct );	// TLB ops, ERET
				return buf;
			}
			// Floating point: rs selects the format
			const char * fmt = "?";
			switch( op.rs )
			{
			case 16: fmt = "S"; break;
			case 17: fmt = "D"; break;
			case 20: fmt = "W"; break;
			case 21: fmt = "L"; break;
			default: break;
			}
			return std::string( kCop1Funct[ funct ] ) + "." + fmt;
		}
	default:
		return kMain[ op.op ];
	}
}

static void WriteFallbacks()
{
	if( gFallbackPath.empty() )
		return;

	std::vector< std::pair< u32, FallbackStats > > list( gFallbacks.begin(), gFallbacks.end() );
	std::sort( list.begin(), list.end(), []( const auto & a, const auto & b ) { return a.second.Count > b.second.Count; } );

	u64 total = 0;
	for( const auto & it : list )
		total += it.second.Count;

	FILE * fh = fopen( gFallbackPath.string().c_str(), "w" );
	if( fh == nullptr )
		return;

	fprintf( fh, "# Interpreter fallbacks from recompiled code for \"%s\" (crc %08x)\n",
			 g_ROM.settings.GameName.c_str(), g_ROM.mRomID.CRC[ 0 ] );
	fprintf( fh, "# after %u vertical blanks. count = times executed, sites = places in recompiled code\n", gFrameIndex );
	fprintf( fh, "# %-12s %12s %7s %7s  %s\n", "instruction", "count", "share", "sites", "example opcode" );
	for( const auto & it : list )
	{
		fprintf( fh, "  %-12s %12u %6.1f%% %7u  0x%08x\n", FallbackName( it.second.Example ).c_str(), it.second.Count,
				 total > 0 ? 100.0 * f64( it.second.Count ) / f64( total ) : 0.0, it.second.Sites, it.second.Example );
	}
	fclose( fh );
}

void NoteInterpretedPC( u32 pc )
{
	gInterpretedOps++;
	if( !gActive || gPcUsed > kPcTableSize / 2 )
		return;
	u32 slot = ( ( pc >> 2 ) * 2654435761u ) >> 16;
	for( ;; slot = ( slot + 1 ) & ( kPcTableSize - 1 ) )
	{
		if( gPcKey[ slot ] == pc )	{ gPcCount[ slot ]++; return; }
		if( gPcKey[ slot ] == 0 )	{ gPcKey[ slot ] = pc; gPcCount[ slot ] = 1; gPcUsed++; return; }
	}
}

static void WriteInterpretedHot()
{
	if( gFallbackPath.empty() )
		return;
	std::filesystem::path path = gFallbackPath;
	path.replace_filename( path.stem().string().substr( 0, path.stem().string().size() - 10 ) + "_interp_hot.txt" );	// strip "_fallbacks"

	struct Entry { u32 pc; u32 count; };
	std::vector< Entry > all, blocks;
	u64 total = 0;
	std::unordered_map< u32, u32 > by_block;	// 256 byte blocks
	for( u32 i = 0; i < kPcTableSize; ++i )
	{
		if( gPcKey[ i ] == 0 )
			continue;
		all.push_back( { gPcKey[ i ], gPcCount[ i ] } );
		by_block[ gPcKey[ i ] & ~0xffu ] += gPcCount[ i ];
		total += gPcCount[ i ];
	}
	for( const auto & it : by_block )
		blocks.push_back( { it.first, it.second } );
	auto by_count = []( const Entry & a, const Entry & b ) { return a.count > b.count; };
	std::sort( all.begin(), all.end(), by_count );
	std::sort( blocks.begin(), blocks.end(), by_count );

	FILE * fh = fopen( path.string().c_str(), "w" );
	if( fh == nullptr )
		return;
	fprintf( fh, "# Instructions run by the plain interpreter (not recompiled) for \"%s\"\n", g_ROM.settings.GameName.c_str() );
	fprintf( fh, "# %llu interpreted ops recorded, %u distinct addresses\n", (unsigned long long)total, (u32)all.size() );
	fprintf( fh, "# dynarec events: cache flushes: cache instruction %u, write over compiled code %u, too many fragments %u, hot trace table full %u\n",
			 gStats[ kStatFlushICache ], gStats[ kStatFlushOverlap ], gStats[ kStatFlushSize ], gStats[ kStatFlushHotMap ] );
	fprintf( fh, "# dynarec events: traces started %u, aborted %u, fragments compiled %u\n",
			 gStats[ kStatTraceStarted ], gStats[ kStatTraceAborted ], gStats[ kStatFragmentCreated ] );
	fprintf( fh, "# audio underruns (output ran dry, heard as crackle): %u\n", gStats[ kStatAudioUnderrun ] );
	fprintf( fh, "# Hottest 256 byte blocks (a function or loop usually lives in one or two):\n" );
	for( size_t i = 0; i < blocks.size() && i < 30; ++i )
		fprintf( fh, "  block %08x  %10u  %5.1f%%\n", blocks[ i ].pc, blocks[ i ].count, 100.0 * blocks[ i ].count / (double)total );
	fprintf( fh, "# Hottest single addresses:\n" );
	for( size_t i = 0; i < all.size() && i < 40; ++i )
		fprintf( fh, "  %08x  %10u  %5.1f%%\n", all[ i ].pc, all[ i ].count, 100.0 * all[ i ].count / (double)total );
	fclose( fh );
}

void NoteFallbackSite( u32 op_code_bits )
{
	FallbackStats & stats = gFallbacks[ FallbackKey( op_code_bits ) ];
	stats.Sites++;
	stats.Example = op_code_bits;
}

// Recompiled code calls this instead of the interpreter handler when the
// perf log is enabled. It counts the instruction and then runs the real
// handler, so the emulated result is the same; only slower.
void FallbackThunk( u32 op_code_bits, void ( *handler )( u32 ) )
{
	gFallbackOps++;
	FallbackStats & stats = gFallbacks[ FallbackKey( op_code_bits ) ];
	stats.Count++;
	if( stats.Example == 0 )
		stats.Example = op_code_bits;

	handler( op_code_bits );
}

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

#ifdef DAEDALUS_PSP
// Time a fixed loop of dependent instructions. The absolute number means
// little, but it scales with the real CPU clock: run it on a stock 333 MHz
// console and again with an overclock plugin active, and the ratio of the two
// is the real speed up. (scePowerGetCpuClockFrequency only reports what the
// system believes, which an overclock plugin does not change.)
static u32 CpuLoopMicros()
{
	u32 n = 20000000;
	u32 t0 = sceKernelGetSystemTimeLow();
	__asm__ volatile ( "1: addiu %0, %0, -1\n bnez %0, 1b\n nop\n" : "+r" ( n ) );
	return sceKernelGetSystemTimeLow() - t0;
}

static void WriteSystemInfo( std::string & out )
{
	static const char * const kModels[] = { "PSP-1000", "PSP-2000", "PSP-3000", "PSP-3000 (04g)", "PSP Go", "unknown" };
	int m = ( gPspModel >= 0 && gPspModel <= 4 ) ? gPspModel : 5;
	char line[ 320 ];
	snprintf( line, sizeof( line ), "# system: model %s (kuKernelGetModel=%d), 64MB model %s, system reports cpu %d MHz bus %d MHz\n",
			  kModels[ m ], gPspModel, PSP_IS_SLIM ? "yes" : "no", scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency() );
	out += line;
	snprintf( line, sizeof( line ), "# system: cpu speed test (20M loop iterations) %u us, %u us, %u us (compare between runs, lower is faster)\n",
			  CpuLoopMicros(), CpuLoopMicros(), CpuLoopMicros() );
	out += line;
}
#endif

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
#ifdef DAEDALUS_PSP
	WriteSystemInfo( gPending );
#endif

	gPending += "vi,flips,total";
	for( u32 i = 0; i < kNumCategories; ++i )
	{
		gPending += ',';
		gPending += kCategoryNames[ i ];
	}
	gPending += ",interp_ops,fallback_ops\n";

	gFallbacks.clear();
	memset( gPcKey, 0, sizeof( gPcKey ) );
	memset( gPcCount, 0, sizeof( gPcCount ) );
	memset( gStats, 0, sizeof( gStats ) );
	gPcUsed = 0;
	gFallbackPath = dir / ( SafeFileName( g_ROM.settings.GameName ) + crc + "_fallbacks.txt" );
	gInterpretedOps = 0;
	gFallbackOps = 0;

	for( u32 i = 0; i < kNumCategories; ++i )
		gTotalMicros[ i ] = 0;
	gTotalFrameMicros = 0;
	gTotalFlips = 0;
	gTotalInterpreted = 0;
	gTotalFallback = 0;
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
	if( len > 0 && len < (int)sizeof( line ) )
		snprintf( line + len, sizeof( line ) - len, ",%u,%u", gInterpretedOps, gFallbackOps );
	gPending += line;
	gPending += '\n';
	gTotalInterpreted += gInterpretedOps;
	gTotalFallback += gFallbackOps;
	gInterpretedOps = 0;
	gFallbackOps = 0;

	gTotalFrameMicros += total;
	gTotalFlips += gFlipsThisFrame;
	gFrameIndex++;

	ResetFrame( now );

	// Write after resetting, so the write lands in the next line's log_io column
	if( gPending.size() >= kFlushThreshold )
		Flush();

	if( ( gFrameIndex % kFallbackDumpInterval ) == 0 )
	{
		Push( kLogIo );
		WriteFallbacks();
		WriteInterpretedHot();
		Pop();
	}
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

		snprintf( line, sizeof( line ), "# summary: interpreted ops avg %.0f, fallback ops avg %.0f per vertical blank\n",
				  f64( gTotalInterpreted ) / f64( gFrameIndex ), f64( gTotalFallback ) / f64( gFrameIndex ) );
		gPending += line;
	}

	Flush();
	WriteFallbacks();
	WriteInterpretedHot();
	gFallbackPath.clear();
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
