/*
 * Test program for native (recompiled) code in DaedalusX64.
 *
 * native_checksum() is recompiled ahead of time with N64Recomp and built into
 * the PSP executable. helper_mix() is NOT recompiled: when the native code
 * calls it, Daedalus runs it with its own JIT/interpreter (the fallback).
 * main() also runs in the JIT. Results go to g_results, which the state
 * check tool hashes every frame, so a native run and a JIT only run can be
 * compared exactly.
 *
 * Phase 2 overwrites native_checksum's code with native_checksum_v2's code
 * and invalidates the instruction cache, as an overlay load would. The native
 * version must not be used any more: g_results[2] must show v2's result.
 */
typedef unsigned int u32;

#define TABLE_SIZE 64

volatile u32 g_results[8] = { 0 };
u32 g_table[TABLE_SIZE];

__attribute__((noipa)) u32 helper_mix( u32 x )
{
	return ( x * 2654435761u ) ^ ( x >> 13 );
}

__attribute__((noipa, aligned(16))) u32 native_checksum( const u32 * data, u32 n, u32 seed )
{
	u32 h = seed;
	for( u32 i = 0; i < n; ++i )
	{
		h = ( h ^ data[ i ] ) * 16777619u;
		h += ( h << 7 ) | ( h >> 25 );
		if( ( i & 7 ) == 7 )
			h ^= helper_mix( h );
	}
	return h;
}

/* Same size and shape, different constants: the "new overlay" */
__attribute__((noipa, aligned(16))) u32 native_checksum_v2( const u32 * data, u32 n, u32 seed )
{
	u32 h = seed ^ 0x5555;
	for( u32 i = 0; i < n; ++i )
	{
		h = ( h ^ data[ i ] ) * 16777619u;
		h += ( h << 9 ) | ( h >> 23 );
		if( ( i & 7 ) == 7 )
			h ^= helper_mix( h );
	}
	return h;
}
void native_checksum_v2_end( void ) {}

__attribute__((noipa)) void invalidate_icache( void * start, u32 bytes )
{
	for( u32 a = (u32)start & ~15u; a < (u32)start + bytes; a += 16 )
		__asm__ volatile( "cache 0x10, 0(%0)" : : "r"( a ) );	/* Hit Invalidate, instruction cache */
}

int main( void )
{
	for( u32 i = 0; i < TABLE_SIZE; ++i )
		g_table[ i ] = i * 0x9E3779B9u + 0x1234;

	u32 total = 0;
	for( u32 round = 0; round < 50; ++round )	/* called many times: the JIT run compiles it */
		total += native_checksum( g_table, TABLE_SIZE, round );
	g_results[ 0 ] = native_checksum( g_table, TABLE_SIZE, 1 );
	g_results[ 1 ] = total;

	/* Phase 2: replace the code, like an overlay or DMA would */
	u32 * dst = (u32 *)native_checksum;
	const u32 * src = (const u32 *)native_checksum_v2;
	u32 words = ( (u32)native_checksum_v2_end - (u32)native_checksum_v2 ) / 4;
	for( u32 i = 0; i < words; ++i )
		dst[ i ] = src[ i ];
	invalidate_icache( dst, words * 4 );

	g_results[ 2 ] = native_checksum( g_table, TABLE_SIZE, 1 );
	g_results[ 3 ] = native_checksum_v2( g_table, TABLE_SIZE, 1 );	/* expected value for [2] */
	g_results[ 4 ] = 0xD0D0D0D0;	/* done marker */

	for( ;; )
		g_results[ 5 ]++;
	return 0;
}
