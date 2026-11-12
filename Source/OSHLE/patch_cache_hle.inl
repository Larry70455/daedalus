#define TEST_DISABLE_CACHE_FUNCS DAEDALUS_PROFILE(__FUNCTION__);



u32 Patch_osInvalICache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
#ifdef DAEDALUS_ENABLE_DYNAREC
	u32 p = gGPR[REG_a0]._u32_0;
	u32 len = gGPR[REG_a1]._u32_0;

	// The game tells us exactly which range of code changed. Only drop the
	// compiled code if it overlaps that range (CPU_InvalidateICacheRange).
	// This used to throw away ALL compiled code whenever len was 16 KB or more
	// (the size of the N64 instruction cache, where libultra switches to
	// invalidating the cache by index), which made games that call this often,
	// such as Pokemon Stadium 2, recompile everything every few seconds.
	// Code loaded by cartridge DMA is invalidated separately in DMA.cpp.
#ifdef DAEDALUS_PERF_LOG
	// Measurement build only: when this call does not flush, check that no
	// compiled instruction differs from memory (stale code would run).
	if( !gFragmentCache.ShouldInvalidateOnWrite( p, len ) )
	{
		PerfLog::gStats[ PerfLog::kStatInvalChecks ]++;
		PerfLog::gStats[ PerfLog::kStatStaleOps ] += PerfLog::CountStaleCompiledOps();
	}
#endif
	CPU_InvalidateICacheRange(p, len);
#endif

	return PATCH_RET_JR_RA;
}

u32 Patch_osInvalICache_Rugrats()
{
	return Patch_osInvalICache_Mario();
}


u32 Patch_osInvalDCache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}
u32 Patch_osInvalDCache_Rugrats()
{
TEST_DISABLE_CACHE_FUNCS
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}


u32 Patch_osWritebackDCache_Mario()
{
TEST_DISABLE_CACHE_FUNCS
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}
u32 Patch_osWritebackDCache_Rugrats()
{
TEST_DISABLE_CACHE_FUNCS
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}


u32 Patch_osWritebackDCacheAll()
{
TEST_DISABLE_CACHE_FUNCS
	//DBGConsole_Msg(0, "osWritebackDCacheAll()");

	return PATCH_RET_JR_RA;
}
