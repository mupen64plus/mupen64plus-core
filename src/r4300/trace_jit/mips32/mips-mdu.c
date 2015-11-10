/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) multiply/divide unit instructions    *
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

#include "mips-mdu.h"

#include "mips-emit.h"
#include "native-ops.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../r4300.h"

enum TJEmitTraceResult mips32_emit_mfhi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMFHI) {
		return mips32_emit_interpret(state, cache, &TJ_MFHI, 0);
	} else {
		uint8_t rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MFHI");
		if (cache->native_hi) {
			uint8_t nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_mfhi(state, nd);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t nHI_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.hi),
			        nHI_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.hi),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_or(state, nd_lo, nHI_lo, 0);
			mips32_or(state, nd_hi, nHI_hi, 0);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mthi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMTHI) {
		return mips32_emit_interpret(state, cache, &TJ_MTHI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MTHI");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
			mips32_mthi(state, ns);
			mips32_set_hi_native(state, cache);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nHI_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.hi),
			        nHI_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.hi);
			mips32_or(state, nHI_lo, ns_lo, 0);
			mips32_or(state, nHI_hi, ns_hi, 0);
			mips32_unset_hi_native(state, cache);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mflo(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMFLO) {
		return mips32_emit_interpret(state, cache, &TJ_MFLO, 0);
	} else {
		uint8_t rd = RD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MFLO");
		if (cache->native_lo) {
			uint8_t nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_mflo(state, nd);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rd]);
		} else {
			uint8_t nLO_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.lo),
			        nLO_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.lo),
			        nd_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rd]),
			        nd_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);
			mips32_or(state, nd_lo, nLO_lo, 0);
			mips32_or(state, nd_hi, nLO_hi, 0);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mtlo(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMTLO) {
		return mips32_emit_interpret(state, cache, &TJ_MTLO, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MTLO");
		if (IntGetWidth(&state->widths, rs) <= 32) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
			mips32_mtlo(state, ns);
			mips32_set_lo_native(state, cache);
		} else {
			uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
			        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        nLO_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.lo),
			        nLO_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.lo);
			mips32_or(state, nLO_lo, ns_lo, 0);
			mips32_or(state, nLO_hi, ns_hi, 0);
			mips32_unset_lo_native(state, cache);
		}
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mult(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMULT) {
		return mips32_emit_interpret(state, cache, &TJ_MULT, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MULT");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_mult(state, ns, nt);
		mips32_set_hi_native(state, cache);
		mips32_set_lo_native(state, cache);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_multu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMULTU) {
		return mips32_emit_interpret(state, cache, &TJ_MULTU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MULTU");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_multu(state, ns, nt);
		mips32_set_hi_native(state, cache);
		mips32_set_lo_native(state, cache);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_div(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDIV) {
		return mips32_emit_interpret(state, cache, &TJ_DIV, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DIV");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_div(state, ns, nt);
		mips32_set_hi_native(state, cache);
		mips32_set_lo_native(state, cache);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_divu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDIVU) {
		return mips32_emit_interpret(state, cache, &TJ_DIVU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DIVU");
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_divu(state, ns, nt);
		mips32_set_hi_native(state, cache);
		mips32_set_lo_native(state, cache);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dmult(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DMULT, 0);
}

enum TJEmitTraceResult mips32_emit_dmultu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDMULTU) {
		return mips32_emit_interpret(state, cache, &TJ_DMULTU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DMULTU");
		uint8_t ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]),
		        ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
		        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        /* Result registers */
		        nr4 = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.hi),
		        nr3 = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.hi),
		        nr2 = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.lo),
		        nr1 = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.lo),
		        /* Temporary registers */
		        nt1 = mips32_alloc_int_temp(state, cache),
		        nt2 = mips32_alloc_int_temp(state, cache);
		/* The multiplication goes like this:
		 *                ns_hi ns_lo
		 * x              nt_hi nt_lo
		 * --------------------------
		 *               [ns_lo*nt_lo]
		 * +       [ns_lo*nt_hi]
		 * +       [ns_hi*nt_lo]
		 * + [ns_hi*nt_hi]
		 * --------------------------
		 *     nr4   nr3   nr2   nr1
		 */
		mips32_multu(state, ns_lo, nt_lo);
		mips32_mflo(state, nr1);
		mips32_mfhi(state, nr2);

		mips32_multu(state, ns_lo, nt_hi);
		mips32_mflo(state, nt1); /* goes into nr2 with carry into nr3 */
		mips32_mfhi(state, nr3);

		mips32_multu(state, ns_hi, nt_lo);
		/* Continue the previous calculation while waiting for this MULTU */
		mips32_addu(state, nr2, nr2, nt1);
		/* Carry into nr3 if nr2 + nt1 overflowed (i.e. nr2 < nt1). */
		mips32_sltu(state, nt1, nr2, nt1);
		mips32_addu(state, nr3, nr3, nt1);
		mips32_mflo(state, nt1); /* goes into nr2 with carry into nr3 */
		mips32_mfhi(state, nt2); /* goes into nr3 with carry into nr4 */

		mips32_multu(state, ns_hi, nt_hi);
		/* Continue the previous calculation while waiting for this MULTU */
		mips32_addu(state, nr2, nr2, nt1);
		/* Carry into nr3 if nr2 + nt1 overflowed (i.e. nr2 < nt1). */
		mips32_sltu(state, nt1, nr2, nt1);
		mips32_addu(state, nr3, nr3, nt1);

		mips32_addu(state, nr3, nr3, nt2);
		/* Carry into nr4 (initialise it to 1) if nr3 + nt2 overflowed
		 * (i.e. nr3 < nt2). Otherwise, it's 0. */
		mips32_sltu(state, nr4, nr3, nt2);

		/* The final calculation for ns_hi * nt_hi starts here. */
		mips32_mflo(state, nt1); /* goes into nr3 with carry into nr4 */
		mips32_mfhi(state, nt2); /* goes into nr4 (possibly added to 1) */

		mips32_addu(state, nr3, nr3, nt1);
		mips32_addu(state, nr4, nr4, nt2);
		/* Carry into nr4 if nr3 + nt1 overflowed (i.e. nr3 < nt1). */
		mips32_sltu(state, nt1, nr3, nt1);
		mips32_addu(state, nr4, nr4, nt1);

		mips32_unset_hi_native(state, cache);
		mips32_unset_lo_native(state, cache);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ddiv(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DDIV, 0);
}

enum TJEmitTraceResult mips32_emit_ddivu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_DDIVU, 0);
}
