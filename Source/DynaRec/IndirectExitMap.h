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

#ifndef DYNAREC_INDIRECTEXITMAP_H_
#define DYNAREC_INDIRECTEXITMAP_H_

#include "Base/Types.h"

class CFragment;
class CFragmentCache;

class CIndirectExitMap
{
	public:
		CIndirectExitMap();
		~CIndirectExitMap();

		CFragment *				LookupIndirectExit( u32 exit_address );
		void					SetCache( const CFragmentCache * p_cache )				{ mpCache = p_cache; }

		// One entry cache for generated code (CCodeGeneratorPSP::GenerateIndirectExitCode):
		// the last target address and the code of the fragment found for it.
		// Generated code reads these two words directly, so keep them first.
		u32						mLastAddress;
		const void *			mLastTarget;

	private:
		const CFragmentCache *	mpCache;
};

//
//	C-stub to allow easy access from dynarec code
//
extern "C" { const void *	 IndirectExitMap_Lookup( CIndirectExitMap * p_map, u32 exit_address ); }
// As above, and remembers a hit in mLastAddress / mLastTarget
extern "C" { const void *	 IndirectExitMap_LookupAndCache( CIndirectExitMap * p_map, u32 exit_address ); }

#endif // DYNAREC_INDIRECTEXITMAP_H_
