/*
Copyright (C) 2005 StrmnNrmn

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
#include "AssemblyWriterPSP.h"

#include "Debug/Registers.h"

#include "Ultra/ultra_R4300.h"

#include <limits>


//	Get the opcodes for loading a 32 bit constant into the specified
//	register. If this can be performed in a single op, the second opcode is NOP.
//	This is useful for splitting a constant load and using the branch delay slot.

void	CAssemblyWriterPSP::GetLoadConstantOps( EPspReg reg, s32 value, PspOpCode * p_op1, PspOpCode * p_op2 )
{
	PspOpCode	op1 {};
	PspOpCode	op2 {};

	if ( value >= std::numeric_limits<short>::lowest() && value <= std::numeric_limits<short>::max() )
	{
		// ORI
		op1.op = OP_ADDIU;
		op1.rt = reg;
		op1.rs = PspReg_R0;
		op1.immediate = u16( value );

		// NOP
		op2._u32 = 0;
	}
	else
	{
		// It's too large - we need to load in two parts
		op1.op = OP_LUI;
		op1.rt = reg;
		op1.rs = PspReg_R0;
		op1.immediate = u16( value >> 16 );

		if ( u16( value ) != 0 )
		{
			op2.op = OP_ORI;
			op2.rt = reg;
			op2.rs = reg;
			op2.immediate = u16( value );
		}
		else
		{
			// NOP
			op2._u32 = 0;
		}
	}

	*p_op1 = op1;
	*p_op2 = op2;
}


//

void	CAssemblyWriterPSP::LoadConstant( EPspReg reg, s32 value )
{
	PspOpCode	op1;
	PspOpCode	op2;
	GetLoadConstantOps( reg, value, &op1, &op2 );

	AppendOp( op1 );

	// There may not be a second op if the low bits are 0, or the constant is small
	if( op2._u32 != 0 )
	{
		AppendOp( op2 );
	}
}


//

void	CAssemblyWriterPSP::LoadRegister( EPspReg reg_dst, OpCodeValue load_op, EPspReg reg_base, s16 offset )
{
	PspOpCode		op_code;
	op_code._u32 = 0;

	op_code.op = load_op;
	op_code.rt = reg_dst;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::StoreRegister( EPspReg reg_src, OpCodeValue store_op, EPspReg reg_base, s16 offset )
{
	PspOpCode		op_code;
	op_code._u32 = 0;

	op_code.op = store_op;
	op_code.rt = reg_src;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::NOP()
{
	PspOpCode		op_code;
	op_code._u32 = 0;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SW( EPspReg reg_src, EPspReg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SW, reg_base, offset );
}


//

void	CAssemblyWriterPSP::SH( EPspReg reg_src, EPspReg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SH, reg_base, offset );
}


//

void	CAssemblyWriterPSP::SB( EPspReg reg_src, EPspReg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SB, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LB( EPspReg reg_dst, EPspReg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LB, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LBU( EPspReg reg_dst, EPspReg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LBU, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LH( EPspReg reg_dst, EPspReg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LH, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LHU( EPspReg reg_dst, EPspReg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LHU, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LW( EPspReg reg_dst, EPspReg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LW, reg_base, offset );
}


//

void	CAssemblyWriterPSP::LWC1( EPspFloatReg reg_dst, EPspReg reg_base, s16 offset )
{
	PspOpCode		op_code;
	op_code._u32 = 0;

	op_code.op = OP_LWC1;
	op_code.ft = reg_dst;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SWC1( EPspFloatReg reg_src, EPspReg reg_base, s16 offset )
{
	PspOpCode		op_code;
	op_code._u32 = 0;

	op_code.op = OP_SWC1;
	op_code.ft = reg_src;
	op_code.base = reg_base;
	op_code.immediate = offset;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::LUI( EPspReg reg, u16 value )
{
	PspOpCode	op_code {};
	op_code.op = OP_LUI;
	op_code.rt = reg;
	op_code.rs = PspReg_R0;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

CJumpLocation	CAssemblyWriterPSP::JAL( CCodeLabel target, bool insert_delay )
{
	CJumpLocation	jump_location( mpCurrentBuffer->GetJumpLocation() );

	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_JAL;
	op_code.target = reinterpret_cast<u32>(target.GetTargetU8P()) >> 2;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return jump_location;
}


//

CJumpLocation	CAssemblyWriterPSP::J( CCodeLabel target, bool insert_delay )
{
	CJumpLocation	jump_location( mpCurrentBuffer->GetJumpLocation() );

	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_J;
	op_code.target = reinterpret_cast<u32>(target.GetTargetU8P()) >> 2;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return jump_location;
}


//

void 	CAssemblyWriterPSP::JR( EPspReg reg_link, bool insert_delay )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.spec_op = SpecOp_JR;
	op_code.rs = reg_link;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}
}


//


//*****************************************************************************
// Branch delay slot filling (DAEDALUS_FILL_BRANCH_DELAY)
//
// A conditional branch emitted with insert_delay used to get a NOP in its
// delay slot: about 5% of all executed generated instructions in a Pokemon
// Stadium 2 battle, and code size (instruction cache misses). When the
// instruction just before the branch can safely run after it instead, the
// branch is written in its place and that instruction goes into the delay
// slot. It still runs exactly once on both paths, before any code at the
// branch target, so the result is the same. It is moved only when:
//  - nothing can jump to it or patch it: no label or jump location was
//    taken at or after its position (CAssemblyBuffer::GetLastMarkPos), which
//    also keeps fragment entry points and loop tops in place;
//  - it is a plain ALU, load or store instruction that we can decode (not a
//    branch, jump, syscall or anything unknown) and is not itself in the
//    delay slot of a branch or jump;
//  - it does not write a register the branch reads.
// Branch likely instructions annul their delay slot, so they keep the NOP.
//*****************************************************************************
#ifndef DAEDALUS_FILL_BRANCH_DELAY
#define DAEDALUS_FILL_BRANCH_DELAY 1
#endif

#if DAEDALUS_FILL_BRANCH_DELAY
// True for any branch or jump (anything with a delay slot)
static bool IsControlTransfer( u32 w )
{
	const u32 op = w >> 26;
	if( op == OP_SPECOP )
	{
		const u32 fn = w & 0x3f;
		return fn == SpecOp_JR || fn == SpecOp_JALR;
	}
	if( op == OP_REGIMM || op == OP_J || op == OP_JAL ) return true;
	if( op >= OP_BEQ && op <= OP_BGTZ ) return true;			// beq bne blez bgtz
	if( op >= OP_BEQL && op <= OP_BGTZL ) return true;		// likely versions
	if( ( op == OP_COPRO1 || op == OP_COPRO1 + 1 ) && ( ( w >> 21 ) & 0x1f ) == 8 ) return true;	// bc1x, VFPU bvf/bvt
	return false;
}

// The general purpose register an instruction writes (0 for none). Returns
// false for anything it does not know, which is then never moved.
static bool GetWrittenRegister( u32 w, u32 * p_reg )
{
	const u32 op = w >> 26;
	const u32 rt = ( w >> 16 ) & 0x1f;
	const u32 rd = ( w >> 11 ) & 0x1f;
	switch( op )
	{
	case OP_SPECOP:
		switch( w & 0x3f )
		{
		case SpecOp_SLL: case SpecOp_SRL: case SpecOp_SRA:
		case SpecOp_SLLV: case SpecOp_SRLV: case SpecOp_SRAV:
		case SpecOp_MFHI: case SpecOp_MFLO:
		case SpecOp_ADD: case SpecOp_ADDU: case SpecOp_SUB: case SpecOp_SUBU:
		case SpecOp_AND: case SpecOp_OR: case SpecOp_XOR: case SpecOp_NOR:
		case SpecOp_SLT: case SpecOp_SLTU:
			*p_reg = rd; return true;
		case SpecOp_MULT: case SpecOp_MULTU: case SpecOp_DIV: case SpecOp_DIVU:
		case SpecOp_MTHI: case SpecOp_MTLO:
			*p_reg = 0; return true;		// only hi/lo
		default:
			return false;
		}
	case OP_ADDI: case OP_ADDIU: case OP_SLTI: case OP_SLTIU:
	case OP_ANDI: case OP_ORI: case OP_XORI: case OP_LUI:
	case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
		*p_reg = rt; return true;
	case OP_SB: case OP_SH: case OP_SW: case OP_SWC1:
		*p_reg = 0; return true;
	case OP_LWC1:
		*p_reg = 0; return true;		// writes a float register
	default:
		return false;
	}
}

// Second try (DAEDALUS_FILL_BRANCH_DELAY_2): often the instruction just
// before the branch computes what the branch tests (the event countdown
// "addiu s5,s5,-n; blez s5", or a constant for a compare), so it cannot
// move. The one before it can then go into the delay slot instead, moving
// past both: allowed only when neither touches a register the other uses,
// at most one of them reads or writes memory, and it does not write a
// register the branch reads. Register numbers 32 (hi/lo) and 33 (all float
// registers) stand for the extra state the decoded instructions use.
#ifndef DAEDALUS_FILL_BRANCH_DELAY_2
#define DAEDALUS_FILL_BRANCH_DELAY_2 1
#endif

static const u32 kRegHiLo = 32;
static const u32 kRegFloat = 33;

// Registers an instruction reads and writes (bit masks over 0..33) and
// whether it accesses memory. False for anything not decoded here.
static bool GetRegisterUse( u32 w, u64 * p_reads, u64 * p_writes, bool * p_memory )
{
	const u32 op = w >> 26;
	const u64 rs = 1ull << ( ( w >> 21 ) & 0x1f );
	const u64 rt = 1ull << ( ( w >> 16 ) & 0x1f );
	const u64 rd = 1ull << ( ( w >> 11 ) & 0x1f );
	const u64 hilo = 1ull << kRegHiLo;
	const u64 fpr = 1ull << kRegFloat;
	*p_memory = false;
	switch( op )
	{
	case OP_SPECOP:
		switch( w & 0x3f )
		{
		case SpecOp_SLL: case SpecOp_SRL: case SpecOp_SRA:
			*p_reads = rt; *p_writes = rd; break;
		case SpecOp_SLLV: case SpecOp_SRLV: case SpecOp_SRAV:
		case SpecOp_ADD: case SpecOp_ADDU: case SpecOp_SUB: case SpecOp_SUBU:
		case SpecOp_AND: case SpecOp_OR: case SpecOp_XOR: case SpecOp_NOR:
		case SpecOp_SLT: case SpecOp_SLTU:
			*p_reads = rs | rt; *p_writes = rd; break;
		case SpecOp_MFHI: case SpecOp_MFLO:
			*p_reads = hilo; *p_writes = rd; break;
		case SpecOp_MTHI: case SpecOp_MTLO:
			*p_reads = rs; *p_writes = hilo; break;
		case SpecOp_MULT: case SpecOp_MULTU: case SpecOp_DIV: case SpecOp_DIVU:
			*p_reads = rs | rt; *p_writes = hilo; break;
		default:
			return false;
		}
		break;
	case OP_ADDI: case OP_ADDIU: case OP_SLTI: case OP_SLTIU:
	case OP_ANDI: case OP_ORI: case OP_XORI:
		*p_reads = rs; *p_writes = rt; break;
	case OP_LUI:
		*p_reads = 0; *p_writes = rt; break;
	case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
		*p_reads = rs; *p_writes = rt; *p_memory = true; break;
	case OP_SB: case OP_SH: case OP_SW:
		*p_reads = rs | rt; *p_writes = 0; *p_memory = true; break;
	case OP_LWC1:
		*p_reads = rs; *p_writes = fpr; *p_memory = true; break;
	case OP_SWC1:
		*p_reads = rs | fpr; *p_writes = 0; *p_memory = true; break;
	default:
		return false;
	}
	*p_reads &= ~1ull;		// r0 is constant
	*p_writes &= ~1ull;
	return true;
}
#endif

// When the previous instruction in the current buffer can go into the delay
// slot of a branch that reads registers a and b: removes it from the buffer
// and returns it (the caller appends it after the branch). Otherwise returns
// false and leaves the buffer alone.
bool	CAssemblyWriterPSP::TakeDelaySlotOp( EPspReg a, EPspReg b, PspOpCode * p_op )
{
#if DAEDALUS_FILL_BRANCH_DELAY
	const u32 pos = mpCurrentBuffer->GetSize();
	if( pos < 8 ) return false;
	const u32 prev_pos = pos - 4;
	if( mpCurrentBuffer->GetLastMarkPos() >= prev_pos ) return false;	// may be jumped to or patched
	const u32 prev = mpCurrentBuffer->GetWordAt( prev_pos );
	if( prev == 0 ) return false;
	if( IsControlTransfer( mpCurrentBuffer->GetWordAt( prev_pos - 4 ) ) ) return false;	// it is a delay slot
	if( IsControlTransfer( prev ) ) return false;
	u32 written;
	if( !GetWrittenRegister( prev, &written ) ) return false;
	if( written == 0 || ( written != u32( a ) && written != u32( b ) ) )
	{
		mpCurrentBuffer->Truncate( prev_pos );
		p_op->_u32 = prev;
		return true;
	}
#if DAEDALUS_FILL_BRANCH_DELAY_2
	// prev computes what the branch tests: try the instruction before it
	if( pos < 12 ) return false;
	const u32 prev2_pos = pos - 8;
	if( mpCurrentBuffer->GetLastMarkPos() >= prev2_pos ) return false;
	const u32 prev2 = mpCurrentBuffer->GetWordAt( prev2_pos );
	if( prev2 == 0 || IsControlTransfer( prev2 ) ) return false;
	if( IsControlTransfer( mpCurrentBuffer->GetWordAt( prev2_pos - 4 ) ) ) return false;	// it is a delay slot
	u64 r1, w1, r2, w2;
	bool m1, m2;
	if( !GetRegisterUse( prev, &r1, &w1, &m1 ) || !GetRegisterUse( prev2, &r2, &w2, &m2 ) ) return false;
	if( m1 && m2 ) return false;					// keep the order of memory accesses
	if( ( w2 & ( r1 | w1 ) ) || ( r2 & w1 ) ) return false;	// they must not depend on each other
	const u64 branch_reads = ( ( 1ull << u32( a ) ) | ( 1ull << u32( b ) ) ) & ~1ull;
	if( w2 & branch_reads ) return false;
	mpCurrentBuffer->Truncate( prev2_pos );
	PspOpCode moved {};
	moved._u32 = prev;
	AppendOp( moved );
	p_op->_u32 = prev2;
	return true;
#else
	return false;
#endif
#else
	return false;
#endif
}

CJumpLocation	CAssemblyWriterPSP::BranchOp( EPspReg a, OpCodeValue op, EPspReg b, CCodeLabel target, bool insert_delay )
{
	PspOpCode		delay_op {};
	const bool		likely( op == OP_BEQL || op == OP_BNEL || op == OP_BLEZL || op == OP_BGTZL );
	const bool		filled( insert_delay && !likely && TakeDelaySlotOp( a, b, &delay_op ) );
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = op;
	op_code.rs = a;
	op_code.rt = b;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if( filled )
	{
		AppendOp( delay_op );
	}
	else if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPSP::BNE( EPspReg a, EPspReg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BNE, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BEQ( EPspReg a, EPspReg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BEQ, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BNEL( EPspReg a, EPspReg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BNEL, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BEQL( EPspReg a, EPspReg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BEQL, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BLEZ( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BLEZ, PspReg_R0, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BGTZ( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BGTZ, PspReg_R0, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BranchRegImmOp( EPspReg a, ERegImmOp op, CCodeLabel target, bool insert_delay )
{
	PspOpCode		delay_op {};
	const bool		likely( op == RegImmOp_BLTZL || op == RegImmOp_BGEZL || op == RegImmOp_BLTZALL || op == RegImmOp_BGEZALL );
	const bool		link( op == RegImmOp_BLTZAL || op == RegImmOp_BGEZAL || op == RegImmOp_BLTZALL || op == RegImmOp_BGEZALL );
	const bool		filled( insert_delay && !likely && !link && TakeDelaySlotOp( a, PspReg_R0, &delay_op ) );
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_REGIMM;
	op_code.rs = a;
	op_code.regimm_op = op;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if( filled )
	{
		AppendOp( delay_op );
	}
	else if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPSP::BLTZ( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BLTZ, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BGEZ( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BGEZ, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BLTZL( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BLTZL, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BGEZL( EPspReg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BGEZL, target, insert_delay );
}



//

void	CAssemblyWriterPSP::ADDI( EPspReg reg_dst, EPspReg reg_src, s16 value )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_ADDI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::ADDIU( EPspReg reg_dst, EPspReg reg_src, s16 value )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_ADDIU;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SLTI( EPspReg reg_dst, EPspReg reg_src, s16 value )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SLTI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SLTIU( EPspReg reg_dst, EPspReg reg_src, s16 value )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SLTIU;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//EXTract bit field. 0 >= pos/size <= 31  //Corn

void	CAssemblyWriterPSP::EXT( EPspReg reg_dst, EPspReg reg_src, u32 size, u32 lsb )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = 0x1F;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.rd = size;	// = MSB - LSB
	op_code.sa = lsb;	// = LSB
	op_code.spec_op = 0;
	AppendOp( op_code );
}


//INSert bit field. 0 >= pos/size <= 31 //Corn

void	CAssemblyWriterPSP::INS( EPspReg reg_dst, EPspReg reg_src, u32 msb, u32 lsb )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = 0x1F;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.rd = msb;	// = MSB
	op_code.sa = lsb;	// = LSB
	op_code.spec_op = 4;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::ANDI( EPspReg reg_dst, EPspReg reg_src, u16 immediate )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_ANDI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::ORI( EPspReg reg_dst, EPspReg reg_src, u16 immediate )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_ORI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::XORI( EPspReg reg_dst, EPspReg reg_src, u16 immediate )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_XORI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SLL( EPspReg reg_dst, EPspReg reg_src, u32 shift )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SLL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SRL( EPspReg reg_dst, EPspReg reg_src, u32 shift )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SRL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SRA( EPspReg reg_dst, EPspReg reg_src, u32 shift )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SRA;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::SpecOpLogical( EPspReg rd, EPspReg rs, ESpecOp op, EPspReg rt )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = rd;
	op_code.rs = rs;
	op_code.rt = rt;
	op_code.spec_op = op;
	AppendOp( op_code );
}


//RD = RT << RS

void	CAssemblyWriterPSP::SLLV( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPSP::SRLV( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SRLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPSP::SRAV( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SRAV, rt );
}


//

void	CAssemblyWriterPSP::MFLO( EPspReg rd )
{
	SpecOpLogical( rd, PspReg_R0, SpecOp_MFLO, PspReg_R0 );
}


//

void	CAssemblyWriterPSP::MFHI( EPspReg rd )
{
	SpecOpLogical( rd, PspReg_R0, SpecOp_MFHI, PspReg_R0 );
}


//

void	CAssemblyWriterPSP::MULT( EPspReg rs, EPspReg rt )
{
	SpecOpLogical( PspReg_R0, rs, SpecOp_MULT, rt );
}


//

void	CAssemblyWriterPSP::MULTU( EPspReg rs, EPspReg rt )
{
	SpecOpLogical( PspReg_R0, rs, SpecOp_MULTU, rt );
}


//

void	CAssemblyWriterPSP::DIV( EPspReg rs, EPspReg rt )
{
	SpecOpLogical( PspReg_R0, rs, SpecOp_DIV, rt );
}


//

void	CAssemblyWriterPSP::DIVU( EPspReg rs, EPspReg rt )
{
	SpecOpLogical( PspReg_R0, rs, SpecOp_DIVU, rt );
}


//

void	CAssemblyWriterPSP::ADD( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_ADD, rt );
}


//

void	CAssemblyWriterPSP::ADDU( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_ADDU, rt );
}


//

void	CAssemblyWriterPSP::SUB( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SUB, rt );
}


//

void	CAssemblyWriterPSP::SUBU( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SUBU, rt );
}


//

void	CAssemblyWriterPSP::AND( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_AND, rt );
}


//

void	CAssemblyWriterPSP::OR( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_OR, rt );
}


//

void	CAssemblyWriterPSP::XOR( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_XOR, rt );
}


//

void	CAssemblyWriterPSP::NOR( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_NOR, rt );
}


//

void	CAssemblyWriterPSP::SLT( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLT, rt );
}


//

void	CAssemblyWriterPSP::SLTU( EPspReg rd, EPspReg rs, EPspReg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLTU, rt );
}


//

void	CAssemblyWriterPSP::Cop1Op( ECop1Op cop1_op, EPspFloatReg fd, EPspFloatReg fs, ECop1OpFunction cop1_funct, EPspFloatReg ft )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.fd = fd;
	op_code.fs = fs;
	op_code.ft = ft;
	op_code.cop1_op = cop1_op;
	op_code.cop1_funct = cop1_funct;
	AppendOp( op_code );
}


//	Unary

void	CAssemblyWriterPSP::Cop1Op( ECop1Op cop1_op, EPspFloatReg fd, EPspFloatReg fs, ECop1OpFunction cop1_funct )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.fd = fd;
	op_code.fs = fs;
	op_code.cop1_op = cop1_op;
	op_code.cop1_funct = cop1_funct;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::CFC1( EPspReg rt, EPspFloatReg fs )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_CFC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::MFC1( EPspReg rt, EPspFloatReg fs )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_MFC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::MTC1( EPspFloatReg fs, EPspReg rt )
{
	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_MTC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPSP::ADD_S( EPspFloatReg fd, EPspFloatReg fs, EPspFloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_ADD, ft );
}


//

void	CAssemblyWriterPSP::SUB_S( EPspFloatReg fd, EPspFloatReg fs, EPspFloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_SUB, ft );
}


//

void	CAssemblyWriterPSP::MUL_S( EPspFloatReg fd, EPspFloatReg fs, EPspFloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_MUL, ft );
}


//

void	CAssemblyWriterPSP::DIV_S( EPspFloatReg fd, EPspFloatReg fs, EPspFloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_DIV, ft );
}


//

void	CAssemblyWriterPSP::SQRT_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_SQRT );
}


//

void	CAssemblyWriterPSP::ABS_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_ABS );
}


//

void	CAssemblyWriterPSP::MOV_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_MOV );
}


//

void	CAssemblyWriterPSP::NEG_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_NEG );
}


//

void	CAssemblyWriterPSP::TRUNC_W_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_TRUNC_W );
}


//

void	CAssemblyWriterPSP::FLOOR_W_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_FLOOR_W );
}

//

void	CAssemblyWriterPSP::CVT_W_S( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_CVT_W );
}


//

void	CAssemblyWriterPSP::CMP_S( EPspFloatReg fs, ECop1OpFunction	cmp_op, EPspFloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, PspFloatReg_F00, fs, cmp_op, ft );
}


//

void	CAssemblyWriterPSP::CVT_S_W( EPspFloatReg fd, EPspFloatReg fs )
{
	Cop1Op( Cop1Op_WInstr, fd, fs, Cop1OpFunc_CVT_S );
}


//

CJumpLocation	CAssemblyWriterPSP::BranchCop1( ECop1BCOp bc_op, CCodeLabel target, bool insert_delay )
{
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	PspOpCode	op_code {};
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.cop1_op = Cop1Op_BCInstr;
	op_code.cop1_bc = bc_op;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPSP::BC1F( CCodeLabel target, bool insert_delay )
{
	return BranchCop1( Cop1BCOp_BC1F, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPSP::BC1T( CCodeLabel target, bool insert_delay )
{
	return BranchCop1( Cop1BCOp_BC1T, target, insert_delay );
}
