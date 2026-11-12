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

// Native code: guest functions recompiled ahead of time (N64Recomp, then the
// PSP compiler) and built into the emulator. See NATIVE_CODE.md.
//
// In short: when the dynarec dispatcher reaches a guest address that has no
// compiled fragment, Native_TryInsert checks whether the loaded ROM has a
// native version of a function starting there, and whether the guest code in
// RDRAM still matches it. If so, a fragment that calls the native function is
// inserted (the same kind of fragment the OS function patches use), so it is
// linked, looked up and invalidated exactly like any other compiled code.
// Everything else keeps running through the dynarec or the interpreter.

#ifndef NATIVE_NATIVECODE_H_
#define NATIVE_NATIVECODE_H_

#include "Base/Types.h"

extern bool gNativeCodeEnabled;		// statecheck "native=0" turns it off

struct NativeStats
{
	u32		Inserted;			// native fragments inserted
	u32		HashMismatch;		// guest code differs from the native version: not used
	u32		NativeCalls;		// native functions run
	u32		NativeOps;			// guest instructions run as native code
	u32		GuestCalls;			// calls from native code back into the emulator
	u32		GuestCallsHle;		// ... of which handled by an OS HLE patch
	u32		GuestOps;			// guest instructions interpreted for those calls
};
extern NativeStats gNativeStats;

void		Native_RomOpened( u32 crc1, u32 crc2 );		// picks the module for this ROM
bool		Native_TryInsert( u32 pc );					// true if a native fragment now exists at pc
const char *Native_ModuleName();						// nullptr if the ROM has none

#endif // NATIVE_NATIVECODE_H_
