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

// Description of precompiled ("native") guest functions, shared by the
// generated C files (Tools/native_recomp/gen_module.py) and Daedalus (C++).
//
// A module belongs to one ROM (header CRC1/CRC2). Each function has its guest
// address, its length in instructions and a checksum of its guest code. The
// native version is only used while the guest code in RDRAM still has that
// checksum, so an overlay or DMA that puts other code there turns it off.

#ifndef NATIVE_NATIVEMODULE_H_
#define NATIVE_NATIVEMODULE_H_

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <fenv.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "Native/recomp.h"

typedef struct NativeFunction
{
	uint32_t		vram;			// guest entry address (KSEG0)
	uint32_t		num_ops;		// guest instructions covered (function size / 4)
	uint32_t		code_hash;		// FNV-1a over the guest instruction words
	recomp_func_t *	func;			// the recompiled function
	uint32_t		( *trampoline )( void );	// called from the dynarec fragment, see native_execute
	const char *	name;
} NativeFunction;

typedef struct NativeModule
{
	const char *			name;
	uint32_t				crc1;
	uint32_t				crc2;
	const NativeFunction *	funcs;
	uint32_t				num_funcs;
} NativeModule;

// Called by each generated module at program start
void		native_register_module( const NativeModule * module );

// Runs f with the current Daedalus CPU state (see NativeCode.cpp)
uint32_t	native_execute( const NativeFunction * f );

// Called by native code for a guest function that is not native: runs it in
// Daedalus (HLE patch if there is one, otherwise the interpreter) and returns
uint32_t	native_call_guest( uint32_t vram, uint8_t * rdram, recomp_context * ctx );

#ifdef __cplusplus
}
#endif

#endif // NATIVE_NATIVEMODULE_H_
