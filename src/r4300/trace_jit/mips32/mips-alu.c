/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) arithmetic logic unit instructions   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Nebuleon <nebuleon.fumika@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdint.h>

#include "mips-alu.h"

#include "mips-emit.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../r4300.h"

enum TJEmitTraceResult mips32_emit_nop(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretNOP) {
		return mips32_emit_interpret(state, cache, &TJ_NOP, 0);
	} else {
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sll(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLL) {
		return mips32_emit_interpret(state, cache, &TJ_SLL, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLL");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_sll(state, nd, nt, sa);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_srl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSRL) {
		return mips32_emit_interpret(state, cache, &TJ_SRL, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SRL");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_srl(state, nd, nt, sa);
		mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sra(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSRA) {
		return mips32_emit_interpret(state, cache, &TJ_SRA, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SRA");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_sra(state, nd, nt, sa);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sllv(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLLV) {
		return mips32_emit_interpret(state, cache, &TJ_SLLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLLV");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_sllv(state, nd, nt, ns);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_srlv(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSRLV) {
		return mips32_emit_interpret(state, cache, &TJ_SRLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SRLV");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_srlv(state, nd, nt, ns);
		/* The extension is SE32, due to the possibility that the value is
		 * being shifted right by 0. */
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_srav(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSRAV) {
		return mips32_emit_interpret(state, cache, &TJ_SRAV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SRAV");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_srav(state, nd, nt, ns);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dsllv(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSLLV, 0);
}

enum TJEmitTraceResult mips32_emit_dsrlv(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSRLV, 0);
}

enum TJEmitTraceResult mips32_emit_dsrav(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSRAV, 0);
}

enum TJEmitTraceResult mips32_emit_addu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADDU) {
		return mips32_emit_interpret(state, cache, &TJ_ADDU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ADDU");
		if (rt == 0 && rs == rd) { /* sign-extending one register into itself */
			/* There is no operation, only loading the original value and
			 * declaring that it must be sign-extended. */
			mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_addu(state, nd, ns, nt);
		}
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_add(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADD || TraceJITSettings.InterpretADDU) {
		return mips32_emit_interpret(state, cache, &TJ_ADD, 0);
	} else {
		return mips32_emit_addu(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_subu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSUBU) {
		return mips32_emit_interpret(state, cache, &TJ_SUBU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SUBU");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_subu(state, nd, ns, nt);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sub(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSUB || TraceJITSettings.InterpretSUBU) {
		return mips32_emit_interpret(state, cache, &TJ_SUB, 0);
	} else {
		return mips32_emit_subu(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_and(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretAND) {
		return mips32_emit_interpret(state, cache, &TJ_AND, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "AND");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_and(state, nd, ns, nt);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_and(state, nd_lo, ns_lo, nt_lo);
			mips32_and(state, nd_hi, ns_hi, nt_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_or(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretOR) {
		return mips32_emit_interpret(state, cache, &TJ_OR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "OR");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_or(state, nd, ns, nt);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_or(state, nd_lo, ns_lo, nt_lo);
			mips32_or(state, nd_hi, ns_hi, nt_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_xor(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretXOR) {
		return mips32_emit_interpret(state, cache, &TJ_XOR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "XOR");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_xor(state, nd, ns, nt);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_xor(state, nd_lo, ns_lo, nt_lo);
			mips32_xor(state, nd_hi, ns_hi, nt_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_nor(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretNOR) {
		return mips32_emit_interpret(state, cache, &TJ_NOR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "NOR");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_nor(state, nd, ns, nt);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_nor(state, nd_lo, ns_lo, nt_lo);
			mips32_nor(state, nd_hi, ns_hi, nt_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_slt(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLT) {
		return mips32_emit_interpret(state, cache, &TJ_SLT, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLT");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_slt(state, nd, ns, nt);
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_bne(state, ns_hi, nt_hi, +3); /* to HighUnequal */
			mips32_nop(state);
			/* HighEqual: */
			mips32_bgez(state, 0, +2); /* to End */
			mips32_sltu(state, nd_lo, ns_lo, nt_lo); /* delay */
			/* HighUnequal: */
			mips32_slt(state, nd_lo, ns_hi, nt_hi);
			/* End: */
			/* We had nd_lo allocated as lo32 just in case rd == rs or
			 * rd == rt, in order to make sure rs or rt's top bits were not
			 * prematurely killed. And now we do this... */
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sltu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLTU) {
		return mips32_emit_interpret(state, cache, &TJ_SLTU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLTU");
		if (IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_sltu(state, nd, ns, nt);
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_bne(state, ns_hi, nt_hi, +3); /* to HighUnequal */
			mips32_nop(state);
			/* HighEqual: */
			mips32_bgez(state, 0, +2); /* to End */
			mips32_sltu(state, nd_lo, ns_lo, nt_lo); /* delay */
			/* HighUnequal: */
			mips32_sltu(state, nd_lo, ns_hi, nt_hi);
			/* End: */
			/* We had nd_lo allocated as lo32 just in case rd == rs or
			 * rd == rt, in order to make sure rs or rt's top bits were not
			 * prematurely killed. And now we do this... */
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_daddu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DADDU, 0);
}

enum TJEmitTraceResult mips32_emit_dadd(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DADD, 0);
}

enum TJEmitTraceResult mips32_emit_dsubu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSUBU, 0);
}

enum TJEmitTraceResult mips32_emit_dsub(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSUB, 0);
}

enum TJEmitTraceResult mips32_emit_dsll(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSLL, 0);
}

enum TJEmitTraceResult mips32_emit_dsrl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSRL, 0);
}

enum TJEmitTraceResult mips32_emit_dsra(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DSRA, 0);
}

enum TJEmitTraceResult mips32_emit_dsll32(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDSLL32) {
		return mips32_emit_interpret(state, cache, &TJ_DSLL32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DSLL32");
		uint8_t nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
		        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		mips32_or(state, nd_lo, 0, 0);
		if (sa != 0) {
			mips32_sll(state, nd_hi, nt_lo, sa);
		} else {
			mips32_or(state, nd_hi, nt_lo, 0);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dsrl32(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDSRL32) {
		return mips32_emit_interpret(state, cache, &TJ_DSRL32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DSRL32");
		uint8_t nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
		        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		if (sa != 0) {
			mips32_srl(state, nd_lo, nt_hi, sa);
		} else {
			mips32_or(state, nd_lo, nt_hi, 0);
		}
		mips32_set_int_ze32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dsra32(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDSRA32) {
		return mips32_emit_interpret(state, cache, &TJ_DSRA32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DSRA32");
		uint8_t nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
		        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
		if (sa != 0) {
			mips32_sra(state, nd_lo, nt_hi, sa);
		} else {
			mips32_or(state, nd_lo, nt_hi, 0);
		}
		mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_addiu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADDIU) {
		return mips32_emit_interpret(state, cache, &TJ_ADDIU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ADDIU");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_addiu(state, nt, ns, imm);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_addi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADDI || TraceJITSettings.InterpretADDIU) {
		return mips32_emit_interpret(state, cache, &TJ_ADDI, 0);
	} else {
		return mips32_emit_addiu(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_slti(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLTI) {
		return mips32_emit_interpret(state, cache, &TJ_SLTI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLTI");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_slti(state, nt, ns, imm);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			if (imm & INT16_C(0x8000)) {
				/* Negative immediate. We must check if 'rs' is less than
				 * 0xFFFF_FFFF_FFFF_<imm>, signed. */
				uint8_t n1 = mips32_alloc_int_const(state, cache, UINT32_C(0xFFFFFFFF));
				mips32_bne(state, ns_hi, n1, +3); /* to HighUnequal */
				mips32_nop(state);
				/* HighEqual: */
				mips32_bgez(state, 0, +2); /* to End */
				mips32_sltiu(state, nt_lo, ns_lo, imm); /* delay */
				/* HighUnequal: */
				mips32_slt(state, nt_lo, ns_hi, n1);
				/* End: */
			} else {
				/* Positive immediate. We must check if 'rs' is less than
				 * 0x0000_0000_0000_<imm>, signed. */
				mips32_bne(state, ns_hi, 0, +3); /* to HighUnequal */
				mips32_nop(state);
				/* HighEqual: */
				mips32_bgez(state, 0, +2); /* to End */
				mips32_sltiu(state, nt_lo, ns_lo, imm); /* delay */
				/* HighUnequal: */
				mips32_slt(state, nt_lo, ns_hi, 0);
				/* End: */
			}
			/* We had nt_lo allocated as lo32 just in case rt == rs, in order
			 * to make sure rs's top bits were not prematurely killed. And now
			 * we declare it as zero-extended. */
		}
		mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sltiu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSLTIU) {
		return mips32_emit_interpret(state, cache, &TJ_SLTIU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SLTIU");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_sltiu(state, nt, ns, imm);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache);
			if (imm & INT16_C(0x8000)) {
				/* Negative immediate. We must check if 'rs' is less than
				 * 0xFFFF_FFFF_FFFF_<imm>, unsigned. */
				mips32_sltiu(state, nt1, ns_hi, -1);
				mips32_sltiu(state, nt2, ns_lo, imm);
				mips32_or(state, nt_lo, nt1, nt2);
			} else {
				/* Positive immediate. We must check if 'rs' is less than
				 * 0x0000_0000_0000_<imm>, unsigned. */
				mips32_sltiu(state, nt1, ns_hi, 1);
				mips32_sltiu(state, nt2, ns_lo, imm);
				mips32_and(state, nt_lo, nt1, nt2);
			}
			/* We had nt_lo allocated as lo32 just in case rt == rs, in order
			 * to make sure rs's top bits were not prematurely killed. And now
			 * we declare it as zero-extended. */
		}
		mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_andi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretANDI) {
		return mips32_emit_interpret(state, cache, &TJ_ANDI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ANDI");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_andi(state, nt, ns, imm);
		mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ori(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretORI) {
		return mips32_emit_interpret(state, cache, &TJ_ORI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ORI");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_ori(state, nt_lo, ns_lo, imm);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_ori(state, nt_lo, ns_lo, imm);
			mips32_move(state, nt_hi, ns_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_xori(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretXORI) {
		return mips32_emit_interpret(state, cache, &TJ_XORI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "XORI");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_xori(state, nt_lo, ns_lo, imm);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nt_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_xori(state, nt_lo, ns_lo, imm);
			mips32_move(state, nt_hi, ns_hi);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lui(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLUI) {
		return mips32_emit_interpret(state, cache, &TJ_LUI, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "LUI");
		uint8_t nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_lui(state, nt, imm);
		mips32_set_int_ex32(cache, &g_state.regs.gpr[rt],
			(imm & INT16_C(0x8000)) ? MRIT_MEM_OE32 : MRIT_MEM_ZE32);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_daddiu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DADDIU, 0);
}

enum TJEmitTraceResult mips32_emit_daddi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DADDI, 0);
}
