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

#ifndef DYNAREC_FRAGMENT_H_
#define DYNAREC_FRAGMENT_H_



#include "Core/R4300Instruction.h"

#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/Trace.h"
#include "DynaRec/RegisterSpan.h"

#include <vector>
#include <memory>
//*************************************************************************************
//	Enable this to allow simulation of the buffer rather than direct execution
//*************************************************************************************
//#define FRAGMENT_SIMULATE_EXECUTION

//
//	This determines whether we retain the trace and the branch details
//
#if defined( FRAGMENT_SIMULATE_EXECUTION ) || defined( DAEDALUS_DEBUG_DYNAREC )
	#define FRAGMENT_RETAIN_ADDITIONAL_INFO
#endif

//*************************************************************************************
//
//*************************************************************************************
class CFragmentCache;
class CCodeGenerator;
class CCodeBufferManager;
class CIndirectExitMap;

struct SFragmentPatchDetails
{
	u32				Address;
	CJumpLocation	Jump;
};
using FragmentPatchList = std::vector<SFragmentPatchDetails>;

class CFragment;

// A patched jump from one fragment into another (see CFragmentCache).
// OriginalOp is the jump instruction as generated, before it was patched to
// point at the target fragment, so the link can be undone.
struct SFragmentLink
{
	CJumpLocation	Jump;
	u32				OriginalOp;
	CFragment *		Source;
};

//*************************************************************************************
//
//*************************************************************************************
class CFragment
{
	using TraceBuffer = std::vector<STraceEntry>;
	using BranchBuffer = std::vector<SBranchDetails>;
public:
		CFragment( std::shared_ptr<CCodeBufferManager> p_manager, u32 entry_address, u32 exit_address,
			const TraceBuffer & trace, SRegisterUsageInfo &	register_usage, const BranchBuffer & branch_details, bool need_indirect_exit_map );
#ifdef DAEDALUS_ENABLE_OS_HOOKS
		CFragment(std::shared_ptr<CCodeBufferManager> p_manager, u32 entry_address, u32 input_length, void* function_Ptr);
		void		Assemble( std::shared_ptr<CCodeBufferManager> p_manager, CCodeLabel native_function);
#endif
		~CFragment();

		void		Execute();

#ifdef DAEDALUS_DEBUG_DYNAREC
		void		DumpFragmentInfoHtml( FILE * fh, u64 total_cycles ) const;
#endif

		u32			GetEntryAddress() const						{ return mEntryAddress; }
		CCodeLabel	GetEntryTarget() const						{ return mEntryPoint; }

		u32			GetMemoryUsage() const;
		u32			GetInputLength() const						{ return mInputLength; }
		u32			GetOutputLength() const						{ return mOutputLength; }

		void		SetCache( const CFragmentCache * p_cache );

		// Pages (4 KB, numbered from 0x80000000) of N64 memory this fragment
		// was compiled from, sorted, no duplicates. Used to drop only the
		// fragments that a write to code memory makes stale.
		const std::vector< u16 > &	GetCodePages() const		{ return mCodePages; }
		bool		IsPatchFragment() const						{ return mIsPatchFragment; }
		bool		IsDead() const								{ return mIsDead; }
		void		SetDead()									{ mIsDead = true; }
		std::vector< SFragmentLink > &	GetIncomingLinks()		{ return mIncomingLinks; }
		void		ResetIndirectExitCache();

		const FragmentPatchList &	GetPatchList() const		{ return mPatchList; }
		void		DiscardPatchList()							{ mPatchList.clear(); }

#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
		u32			GetHitCount() const							{ return mHitCount; }
		u32			GetCyclesExecuted() const					{ return mHitCount * mOutputLength / 4; }

		u32			GetExitAddress() const						{ return mExitAddress; }
#endif

private:
		void		Analyse( const std::vector< STraceEntry > & trace, SRegisterUsageInfo & register_usage );
		void		Assemble( std::shared_ptr<CCodeBufferManager> p_manager, u32 exit_address, const std::vector< STraceEntry > & trace, const std::vector<SBranchDetails> & branch_details, const SRegisterUsageInfo & register_usage );

		void		AddPatch( u32 address, CJumpLocation jump_location );

#ifdef FRAGMENT_SIMULATE_EXECUTION
		CFragment *	Simulate();
#endif

private:
		u32								mEntryAddress;

		std::vector< SFragmentPatchDetails >	mPatchList;

		CCodeLabel						mEntryPoint;
		u32								mInputLength;
		u32								mOutputLength;		// Essentially the same as mFragmentFunctionLength, but takes into account additional debugging instructions etc
		u32								mFragmentFunctionLength;

		CIndirectExitMap *				mpIndirectExitMap;

		std::vector< u16 >				mCodePages;
		std::vector< SFragmentLink >	mIncomingLinks;		// jumps from other fragments patched to enter this one
		bool							mIsPatchFragment;
		bool							mIsDead;			// removed from the cache, kept alive until the next Clear

#ifdef FRAGMENT_RETAIN_ADDITIONAL_INFO
		u32								mHitCount;
		TraceBuffer						mTraceBuffer;
		BranchBuffer					mBranchBuffer;

		std::vector< const u8 * >		mInstructionStartLocations;	// For each entry in the trace, this holds the first instruction in the output buffer

		SRegisterUsageInfo				mRegisterUsage;

		u32								mExitAddress;
#endif
#ifdef FRAGMENT_SIMULATE_EXECUTION
		const CFragmentCache *			mpCache;
#endif

};

#endif // DYNAREC_FRAGMENT_H_
