/*
Copyright (C) 2006 StrmnNrmn

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


#include "Base/Types.h"


#include <stdio.h>
#include <algorithm>
#include <cstring>
#include <fstream>

#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/CodeBufferManager.h"
#include "DynaRec/DynaRecProfile.h"
#include "DynaRec/Fragment.h"
#include "DynaRec/FragmentCache.h"
#include "Debug/DBGConsole.h"
#include "Utility/Profiler.h"
#include "Utility/PerfLog.h"


//Define to show hash table statistics cache hit/miss
//#define HASH_TABLE_STATS

using namespace AssemblyUtils;

//*************************************************************************************
//
//*************************************************************************************
CFragmentCache::CFragmentCache()
:	mMemoryUsage( 0 )
,	mInputLength( 0 )
,	mOutputLength( 0 )
,	mCachedFragmentAddress( 0 )
,	mpCachedFragment( nullptr )
{
	std::memset( mpCacheHashTable.data(), 0, mpCacheHashTable.size() * sizeof(mpCacheHashTable[0]));
#if DAEDALUS_SELECTIVE_INVALIDATE
	mPageRefs.fill( 0 );
#endif

	mFragments.reserve( 2000 );

	mpCodeBufferManager = CCodeBufferManager::Create();
	if(mpCodeBufferManager != nullptr)
	{
		mpCodeBufferManager->Initialise();
	}
}

//*************************************************************************************
//
//*************************************************************************************
CFragmentCache::~CFragmentCache()
{
	Clear();

	mpCodeBufferManager->Finalise();
}

//*************************************************************************************
//
//*************************************************************************************
#ifdef DAEDALUS_DEBUG_DYNAREC
CFragment * CFragmentCache::LookupFragment( u32 address ) const
{
	DAEDALUS_PROFILE( "CFragmentCache::LookupFragment" );

	if( address != mCachedFragmentAddress )
	{
		mCachedFragmentAddress = address;

		// check if in hash table
		u32 ix = MakeHashIdx( address );

		if ( address != mpCacheHashTable[ix].addr )
		{
			SFragmentEntry				entry( address, nullptr );
			FragmentVec::const_iterator	it( std::lower_bound( mFragments.begin(), mFragments.end(), entry ) );
			if( it != mFragments.end() && it->Address == address )
			{
				mpCachedFragment = it->Fragment;
			}
			else
			{
				mpCachedFragment = nullptr;
			}

			// put in hash table
			mpCacheHashTable[ix].addr = address;
			mpCacheHashTable[ix].ptr = (uintptr_t)mpCachedFragment;
		}
		else
		{
			mpCachedFragment = (CFragment *)mpCacheHashTable[ix].ptr;
		}
	}

	CFragment * p = mpCachedFragment;

	DYNAREC_PROFILE_LOGLOOKUP( address, p );

	return p;
}
#endif
//*************************************************************************************
//
//*************************************************************************************
CFragment * CFragmentCache::LookupFragmentQ( u32 address ) const
{
	#ifdef DAEDALUS_DEBUG_DYNAREC
	DAEDALUS_PROFILE( "CFragmentCache::LookupFragmentQ" );
	#endif
#ifdef HASH_TABLE_STATS
	static u32 hit=0, miss=0;
#endif
	if( address != mCachedFragmentAddress )
	{
		mCachedFragmentAddress = address;

		// check if in hash table
		u32 ix = MakeHashIdx( address );

		if ( address != mpCacheHashTable[ix].addr )
		{
#ifdef HASH_TABLE_STATS
			miss++;
#endif
			SFragmentEntry				entry( address, nullptr );
			FragmentVec::const_iterator	it( std::lower_bound( mFragments.begin(), mFragments.end(), entry ) );
			if( it != mFragments.end() && it->Address == address )
			{
				mpCachedFragment = it->Fragment;
			}
			else
			{
				mpCachedFragment = nullptr;
			}

			// put in hash table
			mpCacheHashTable[ix].addr = address;
			mpCacheHashTable[ix].ptr = reinterpret_cast< uintptr_t >( mpCachedFragment );
		}
		else
		{
#ifdef HASH_TABLE_STATS
			hit++;
#endif
			mpCachedFragment = reinterpret_cast< CFragment * >( mpCacheHashTable[ix].ptr );
		}

#ifdef HASH_TABLE_STATS
		if(miss+hit==10000)
		{
			printf("Hit[%d]  Miss[%d]\n", hit, miss);
			miss=hit=0;
		}
#endif
	}

	return mpCachedFragment;
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::InsertFragment( CFragment * p_fragment )
{
	CFragment * const	p_new_fragment( p_fragment );	// p_fragment is shadowed below
	(void)p_new_fragment;
	u32		fragment_address( p_fragment->GetEntryAddress() );

#if DAEDALUS_SELECTIVE_INVALIDATE
	for( u16 page : p_fragment->GetCodePages() )
	{
		mPageRefs[ page ]++;
	}
#else
	mCacheCoverage.ExtendCoverage( fragment_address, p_fragment->GetInputLength() );
#endif

	SFragmentEntry				entry( fragment_address, nullptr );
	FragmentVec::iterator		it( std::lower_bound( mFragments.begin(), mFragments.end(), entry ) );
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( it == mFragments.end() || it->Address != fragment_address, "A fragment with this address already exists" );
	#endif
	entry.Fragment = p_fragment;
	mFragments.insert( it, entry );

	// Update the hash table (it stores failed lookups now, so we need to be sure to purge any stale entries in there
	u32 ix {MakeHashIdx( fragment_address )};
	mpCacheHashTable[ix].addr = fragment_address;
	mpCacheHashTable[ix].ptr = reinterpret_cast< uintptr_t >( p_fragment );

	// The last lookup (LookupFragmentQ's one entry cache) may have cached "no
	// fragment" for this address: the dispatcher looks an address up, finds
	// nothing and starts the trace that becomes this fragment. Without this,
	// the fragment's own exits back to its entry (and any other lookup of it
	// until the cache moves on) still saw "no fragment" and were left
	// unlinked, so every pass through such a loop went back to the
	// dispatcher.
	if( mCachedFragmentAddress == fragment_address )
	{
		mpCachedFragment = p_fragment;
	}

	// Process any jumps for this before inserting new ones
	JumpMap::iterator	jump_it( mJumpMap.find( fragment_address ) );
	if( jump_it != mJumpMap.end() )
	{
		const JumpList &		jumps( jump_it->second );
		for( JumpList::const_iterator it = jumps.begin(); it != jumps.end(); ++it )
		{
			//DBGConsole_Msg( 0, "Inserting [R%08x], patching jump at %08x ", address, (*it) );
#if DAEDALUS_SELECTIVE_INVALIDATE
			if( !it->Source->IsDead() )
			{
				LinkJump( it->Jump, it->Source, p_fragment );
			}
#else
			PatchJumpLongAndFlush( (*it), p_fragment->GetEntryTarget() );
#endif
		}

		// All patched - clear
		mJumpMap.erase( jump_it );
	}

	// Finally register any links that this fragment may have
	const FragmentPatchList &	patch_list( p_fragment->GetPatchList() );
	for( FragmentPatchList::const_iterator it = patch_list.begin(); it != patch_list.end(); ++it )
	{
		u32				target_address( it->Address );
		CJumpLocation	jump( it->Jump );

	#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( jump.IsSet(), "No exit jump?" );
		#endif

#ifdef DAEDALUS_DEBUG_DYNAREC
		CFragment * p_fragment( LookupFragment( target_address ) );
#else
		CFragment * p_fragment( LookupFragmentQ( target_address ) );
#endif
		if( p_fragment != nullptr )
		{
#if DAEDALUS_SELECTIVE_INVALIDATE
			LinkJump( jump, p_new_fragment, p_fragment );
#else
			PatchJumpLongAndFlush( jump, p_fragment->GetEntryTarget() );
#endif

	#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( mJumpMap.find( target_address ) == mJumpMap.end(), "Jump map still contains an entry for this" );
			#endif
		}
		else if( target_address != u32(~0) )
		{
			// Store the address for later processing
#if DAEDALUS_SELECTIVE_INVALIDATE
			mJumpMap[ target_address ].push_back( SPendingJump{ jump, p_new_fragment } );
#else
			mJumpMap[ target_address ].push_back( jump );
#endif
		}
	}

	// Free memoire
	p_fragment->DiscardPatchList();

	// For simulation only
	p_fragment->SetCache( this );

	// Update memory usage etc after discarding patch list
	mMemoryUsage += p_fragment->GetMemoryUsage();
	mInputLength += p_fragment->GetInputLength();
	mOutputLength += p_fragment->GetOutputLength();

#ifdef DAEDALUS_DEBUG_CONSOLE
	if((mFragments.size() % 100) == 0)
	{
		u32		expansion = 1;
		if(mInputLength > 0)
		{
			expansion = (100 * mOutputLength) / mInputLength;
		}
		DBGConsole_Msg( 0, "Dynarec: %d fragments, %dKB (i:o %d:%d = %d%%)",
			mFragments.size(), mMemoryUsage / 1024, mInputLength / 1024, mOutputLength / 1024, expansion );
	}
#endif
}

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::Clear()
{
#ifdef DAEDALUS_PERF_LOG
	PerfLog::ClearCompiledOps();	// stale code verifier: the cache is empty again
#endif
#ifdef DAEDALUS_DEBUG_CONSOLE
	if(CDebugConsole::IsAvailable())
	{
		DBGConsole_Msg( 0, "Clearing fragment cache of %d fragments", mFragments.size() );
	}
#endif
	// Clear out all the framents
	for(FragmentVec::iterator it = mFragments.begin(); it != mFragments.end(); ++it)
	{
		delete it->Fragment;
	}

	mFragments.erase( mFragments.begin(), mFragments.end() );
#if DAEDALUS_SELECTIVE_INVALIDATE
	for( CFragment * p_dead : mDeadFragments )
	{
		delete p_dead;
	}
	mDeadFragments.clear();
	mPendingInvalidations.clear();
	mPageRefs.fill( 0 );
#endif
	mMemoryUsage = 0;
	mInputLength = 0;
	mOutputLength = 0;
	mCachedFragmentAddress = 0;
	mpCachedFragment = nullptr;
	std::memset( mpCacheHashTable.data(), 0, mpCacheHashTable.size() * sizeof(mpCacheHashTable[0]));
	mJumpMap.clear();

	mCacheCoverage.Reset();

	mpCodeBufferManager->Reset();
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCache::ShouldInvalidateOnWrite( u32 address, u32 length ) const
{
#if DAEDALUS_SELECTIVE_INVALIDATE
	// Same page numbering and range as CFragmentCacheCoverage
	const u32 num_pages = mPageRefs.size();
	u32 first = ( address - 0x80000000 ) >> 12;
	u32 last = ( address + length - 0x80000000 ) >> 12;
	for( u32 i = first; i <= last && i < num_pages; ++i )
	{
		if( mPageRefs[ i ] != 0 )
			return true;
	}
	return false;
#else
	return mCacheCoverage.IsCovered( address, length );
#endif
}

#if DAEDALUS_SELECTIVE_INVALIDATE
//*************************************************************************************
//	Patch a jump in source to enter target, remembering how to undo it
//*************************************************************************************
void CFragmentCache::LinkJump( CJumpLocation jump, CFragment * source, CFragment * target )
{
	u32 original_op = ReadJumpWord( jump );
	if( PatchJumpLongAndFlush( jump, target->GetEntryTarget() ) )
	{
		target->GetIncomingLinks().push_back( SFragmentLink{ jump, original_op, source } );
	}
}

void CFragmentCache::QueueInvalidation( u32 address, u32 length )
{
	mPendingInvalidations.emplace_back( address, length );
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCache::ProcessPendingInvalidations( std::vector< u32 > & dropped_entries, std::vector< std::pair< u32, u32 > > & ranges )
{
	const u32 num_pages = mPageRefs.size();

	// Pages written to
	std::vector< bool > written( num_pages, false );
	for( const auto & r : mPendingInvalidations )
	{
		u32 first = ( r.first - 0x80000000 ) >> 12;
		u32 last = ( r.first + r.second - 0x80000000 ) >> 12;
		for( u32 i = first; i <= last && i < num_pages; ++i )
		{
			written[ i ] = true;
		}
	}
	ranges.insert( ranges.end(), mPendingInvalidations.begin(), mPendingInvalidations.end() );
	mPendingInvalidations.clear();

	// Fragments compiled from any of those pages
	std::vector< CFragment * > victims;
	for( const SFragmentEntry & e : mFragments )
	{
		for( u16 page : e.Fragment->GetCodePages() )
		{
			if( written[ page ] )
			{
				if( e.Fragment->IsPatchFragment() )
				{
					return false;	// keep the old behaviour for OS patches: full Clear
				}
				victims.push_back( e.Fragment );
				break;
			}
		}
	}
	if( victims.empty() )
	{
		return true;
	}

	for( CFragment * p_victim : victims )
	{
		p_victim->SetDead();
	}

	for( CFragment * p_victim : victims )
	{
		u32 entry = p_victim->GetEntryAddress();
		dropped_entries.push_back( entry );

		for( u16 page : p_victim->GetCodePages() )
		{
			mPageRefs[ page ]--;
		}

		// Undo every link into this fragment, and queue the live ones to be
		// linked again when the code at this address is compiled again
		for( const SFragmentLink & link : p_victim->GetIncomingLinks() )
		{
			if( !link.Source->IsDead() )
			{
				RestoreJumpWord( link.Jump, link.OriginalOp );
				mJumpMap[ entry ].push_back( SPendingJump{ link.Jump, link.Source } );
			}
		}
		p_victim->GetIncomingLinks().clear();

		// Forget cached lookups of it
		u32 ix = MakeHashIdx( entry );
		if( mpCacheHashTable[ ix ].addr == entry )
		{
			mpCacheHashTable[ ix ].addr = 0;
			mpCacheHashTable[ ix ].ptr = 0;
		}

		mMemoryUsage -= p_victim->GetMemoryUsage();
		mInputLength -= p_victim->GetInputLength();
		mOutputLength -= p_victim->GetOutputLength();

#ifdef DAEDALUS_PERF_LOG
		PerfLog::ForgetCompiledOpsInPages( p_victim->GetCodePages() );
#endif
		mDeadFragments.push_back( p_victim );
	}

	// Remove them from the sorted list
	mFragments.erase( std::remove_if( mFragments.begin(), mFragments.end(),
		[]( const SFragmentEntry & e ) { return e.Fragment->IsDead(); } ), mFragments.end() );

	// Pending links that come from dropped fragments will never be needed
	for( JumpMap::iterator it = mJumpMap.begin(); it != mJumpMap.end(); )
	{
		JumpList & list = it->second;
		list.erase( std::remove_if( list.begin(), list.end(),
			[]( const SPendingJump & j ) { return j.Source->IsDead(); } ), list.end() );
		if( list.empty() )
			it = mJumpMap.erase( it );
		else
			++it;
	}

	// Indirect exits may remember a dropped fragment as their last target
	for( const SFragmentEntry & e : mFragments )
	{
		e.Fragment->ResetIndirectExitCache();
	}

	mCachedFragmentAddress = 0;
	mpCachedFragment = nullptr;
	return true;
}
#endif




//*************************************************************************************
//
//*************************************************************************************
#define AddressToIndex( addr ) ((addr - BASE_ADDRESS) >> MEM_USAGE_SHIFT)

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCacheCoverage::ExtendCoverage( u32 address, u32 len )
{
	u32 first_entry = AddressToIndex( address );
	u32 last_entry = AddressToIndex( address + len );

	// Mark all entries as true
	for( u32 i = first_entry; i <= last_entry && i < NUM_MEM_USAGE_ENTRIES; ++i )
	{
		mCacheCoverage[ i ] = true;
	}
}

//*************************************************************************************
//
//*************************************************************************************
bool CFragmentCacheCoverage::IsCovered( u32 address, u32 len ) const
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	if((address - BASE_ADDRESS) == 0)
	{
		DBGConsole_Msg( 0, "Cache coverage address is overlapping" );
		return true;
	}
#endif
	u32 first_entry = AddressToIndex( address );
	u32 last_entry = AddressToIndex( address + len );

	// Mark all entries as true
	for( u32 i = first_entry; i <= last_entry && i < NUM_MEM_USAGE_ENTRIES; ++i )
	{
		if( mCacheCoverage[ i ] )
			return true;
	}

	return false;
}

//*************************************************************************************
//
//*************************************************************************************

#ifdef DAEDALUS_DEBUG_DYNAREC
//*************************************************************************************
//
//*************************************************************************************
struct SDescendingCyclesSort
{
     bool operator()(CFragment* const & a, CFragment* const & b)
     {
		return b->GetCyclesExecuted() < a->GetCyclesExecuted();
     }
};

//*************************************************************************************
//
//*************************************************************************************
void CFragmentCache::DumpStats( const std::filesystem::path outputdir ) const
{

	using FragmentList = std::vector< CFragment *>;
	
	FragmentList		all_fragments;

	all_fragments.reserve( mFragments.size() );

	u32		total_cycles( 0 );

	// Sort in order of expended cycles
	for(FragmentVec::const_iterator it = mFragments.begin(); it != mFragments.end(); ++it)
	{
		all_fragments.push_back( it->Fragment );
		total_cycles += it->Fragment->GetCyclesExecuted();
	}

	std::sort( all_fragments.begin(), all_fragments.end(), SDescendingCyclesSort() );


	std::filesystem::path filename = "fragments.html";
	std::filesystem::path fragments_dir = setbasePath("fragments");
	fragments_dir /= filename;


	std::ofstream fh(filename);
    if (fh.is_open())
	 {
        fh << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
        fh << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
        fh << "<head><title>Fragments</title>\n";
        fh << "<link rel=\"stylesheet\" href=\"default.css\" type=\"text/css\" media=\"all\" />\n";
        fh << "</head><body>\n";

        fh << "<h1>Fragments</h1>\n";
        fh << "<div align=\"center\"><table>\n";
        fh << "<tr><th>Address</th><th>Loops</th><th>Cycle Count</th><th>Cycle %</th><th>Hit Count</th><th>Input Bytes</th><th>Output Bytes</th><th>Expansion Ratio</th></tr>\n";

        for (auto it = all_fragments.begin(); it != all_fragments.end(); ++it) {
            const CFragment* fragment = *it;

            if (fragment->GetCyclesExecuted() == 0)
                continue;

            fh << "<tr>";
            fh << "<td><a href=\"fragments//" << std::hex << std::setw(8) << std::setfill('0') << fragment->GetEntryAddress() << ".html\">"
               << "0x" << std::hex << std::setw(8) << std::setfill('0') << fragment->GetEntryAddress() << "</a></td>";
            fh << "<td>" << (fragment->GetEntryAddress() == fragment->GetExitAddress() ? "*" : "&nbsp;") << "</td>";
            fh << "<td>" << fragment->GetCyclesExecuted() << "</td>";
            fh << "<td>" << std::fixed << std::setprecision(2) << (static_cast<float>(fragment->GetCyclesExecuted()) * 100.0f / total_cycles) << "%</td>";
            fh << "<td>" << fragment->GetHitCount() << "</td>";
            fh << "<td>" << fragment->GetInputLength() << "</td>";
            fh << "<td>" << fragment->GetOutputLength() << "</td>";
            fh << "<td>" << std::fixed << std::setprecision(2) << (static_cast<float>(fragment->GetOutputLength()) / fragment->GetInputLength()) << "</td>";
            fh << "</tr>\n";

            std::string fragment_name = fragments_dir + "/" + std::to_string(fragment->GetEntryAddress()) + ".html";
            std::ofstream fragment_fh(fragment_name);
            if (fragment_fh.is_open()) {
                fragment->DumpFragmentInfoHtml(fragment_fh, total_cycles);
                fragment_fh.close();
            }
        }

        fh << "</table></div>\n";
        fh << "</body></html>\n";
	}
}
#endif // DAEDALUS_DEBUG_DYNAREC

void CFragmentCacheCoverage::Reset( )
{
	std::fill(std::begin(mCacheCoverage), std::end(mCacheCoverage), 0);
		// std::memset( mCacheCoverage.data(), false, mCacheCoverage.size() * sizeof( mCacheCoverage ) );
}
