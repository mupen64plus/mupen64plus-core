/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) Coprocessor 0 instructions           *
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

#include "mips-cp0.h"

#include "mips-emit.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../r4300.h"

static enum TJEmitTraceResult emit_mfc0_general(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MFC0");
	uint8_t nd = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[rd]),
	        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
	mips32_or(state, nt, nd, 0);
	mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

enum TJEmitTraceResult mips32_emit_mfc0(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	switch (RD_OF(state->ops[0])) {
	case CP0_COUNT_REG:
		state->last_count_update_pc = state->pc;
		/* fall through */
	case CP0_RANDOM_REG:
		return mips32_emit_interpret(state, cache, &TJ_MFC0, TJ_READS_PC);
	default:
		return TraceJITSettings.InterpretMFC0
		     ? mips32_emit_interpret(state, cache, &TJ_MFC0, 0)
		     : emit_mfc0_general(state, cache);
	}
}

static void mtc0_index_invalid()
{
	DebugMessage(M64MSG_ERROR, "MTC0 instruction writing Index register with TLB index > 31");
	stop = 1;
}

static enum TJEmitTraceResult mtc0_index_invalid_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	mips32_free_all(state, cache);
	mips32_pic_call(state, &mtc0_index_invalid);
	mips32_jr(state, REG_ESCAPE);
	mips32_nop(state);
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult emit_mtc0_index(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 Index");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t nmask = mips32_alloc_int_const(state, cache, UINT32_C(0x8000003F)),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]),
		        nt1 = mips32_alloc_int_temp(state, cache);
		/* [if ((Index & 0x3F) > 31) ERROR;] */
		mips32_andi(state, nt1, nt, UINT16_C(0x20));
		mips32_bne(state, nt1, 0, +0);
		mips32_add_slow_path(state, cache, &mtc0_index_invalid_slow, state->code,
			NULL /* no usual path */, 0 /* no userdata */);
		mips32_and(state, nd, nt, nmask); /* Index = rt & 0x8000003F; (delay slot) */
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static void mtc0_cause_invalid()
{
	DebugMessage(M64MSG_ERROR, "MTC0 instruction trying to write Cause register with non-0 value");
	stop = 1;
}

static enum TJEmitTraceResult mtc0_cause_invalid_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	mips32_free_all(state, cache);
	mips32_pic_call(state, &mtc0_cause_invalid);
	mips32_jr(state, REG_ESCAPE);
	mips32_nop(state);
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult emit_mtc0_cause(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 Cause");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_bne(state, nt, 0, +0);
		mips32_add_slow_path(state, cache, &mtc0_cause_invalid_slow, state->code,
			NULL /* no usual path */, 0 /* no userdata */);
		mips32_or(state, nd, 0, 0);
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_entrylo0(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 EntryLo0");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]),
		        nt1 = mips32_alloc_int_temp(state, cache);
		mips32_sll(state, nt1, nt, 2);
		mips32_srl(state, nd, nt1, 2); /* EntryLo0 = rt & 0x3FFFFFFF; */
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_entrylo1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 EntryLo1");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]),
		        nt1 = mips32_alloc_int_temp(state, cache);
		mips32_sll(state, nt1, nt, 2);
		mips32_srl(state, nd, nt1, 2); /* EntryLo1 = rt & 0x3FFFFFFF; */
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_context(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 Context");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t n0xff800000 = mips32_alloc_int_const(state, cache, UINT32_C(0xFF800000)),
		        n0x007ffff0 = mips32_alloc_int_const(state, cache, UINT32_C(0x007FFFF0)),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[rd]),
		        nt1 = mips32_alloc_int_temp(state, cache);
		mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_and(state, nd, nd, n0x007ffff0); /* Context & 0x7FFFF0 */
		mips32_and(state, nt1, nt, n0xff800000); /* rt & 0xFF800000 */
		mips32_or(state, nd, nd, nt1);
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_pagemask(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 PageMask");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t n0x01ffe000 = mips32_alloc_int_const(state, cache, UINT32_C(0x01FFE000)),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_and(state, nd, nt, n0x01ffe000);
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_wired(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 Wired");
	uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
	        nwired = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[CP0_WIRED_REG]),
	        nrandom = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[CP0_RANDOM_REG]);
	mips32_or(state, nwired, nt, 0);
	mips32_ori(state, nrandom, 0, UINT16_C(31));
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_entryhi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 EntryHi");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t n0xffffe0ff = mips32_alloc_int_const(state, cache, UINT32_C(0xFFFFE0FF)),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_and(state, nd, nt, n0xffffe0ff);
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_taglo(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 TagLo");
	if (rt == 0) {
		uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_or(state, nd, 0, 0);
	} else {
		uint8_t n0x0fffffc0 = mips32_alloc_int_const(state, cache, UINT32_C(0x0FFFFFC0)),
		        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
		mips32_and(state, nd, nt, n0x0fffffc0);
	}
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_taghi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0 TagHi");
	uint8_t nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
	mips32_or(state, nd, 0, 0);
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_general(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
	mips32_start_opcode(state, cache, "MTC0");
	uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
	        nd = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[rd]);
	mips32_or(state, nd, nt, 0);
	mips32_end_opcode(state, cache);
	return mips32_next_opcode(state);
}

static enum TJEmitTraceResult emit_mtc0_write_ignored(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_next_opcode(state);
}

enum TJEmitTraceResult mips32_emit_mtc0(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	switch (RD_OF(state->ops[0])) {
	case CP0_COUNT_REG:
	case CP0_COMPARE_REG:
		/* Writes to Count and Compare both update the Count value */
		state->last_count_update_pc = state->pc;
		/* fall through */
	case CP0_STATUS_REG:
		return mips32_emit_interpret(state, cache, &TJ_MTC0, TJ_READS_PC | TJ_MAY_RAISE_INTERRUPT);
	case CP0_INDEX_REG:
		if (TraceJITSettings.InterpretMTC0) {
			if (RT_OF(state->ops[0]) != 0) {
				return mips32_emit_interpret(state, cache, &TJ_MTC0, TJ_CHECK_STOP);
			} else {
				return mips32_emit_interpret(state, cache, &TJ_MTC0, 0);
			}
		} else {
			return emit_mtc0_index(state, cache);
		}
	case CP0_CAUSE_REG:
		if (TraceJITSettings.InterpretMTC0) {
			if (RT_OF(state->ops[0]) != 0) {
				return mips32_emit_interpret(state, cache, &TJ_MTC0, TJ_CHECK_STOP);
			} else {
				return mips32_emit_interpret(state, cache, &TJ_MTC0, 0);
			}
		} else {
			return emit_mtc0_cause(state, cache);
		}
	case CP0_ENTRYLO0_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_entrylo0(state, cache);
	case CP0_ENTRYLO1_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_entrylo1(state, cache);
	case CP0_EPC_REG:
	case CP0_CONFIG_REG:
	case CP0_WATCHLO_REG:
	case CP0_WATCHHI_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_general(state, cache);
	case CP0_CONTEXT_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_context(state, cache);
	case CP0_PAGEMASK_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_pagemask(state, cache);
	case CP0_WIRED_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_wired(state, cache);
	case CP0_ENTRYHI_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_entryhi(state, cache);
	case CP0_TAGLO_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_taglo(state, cache);
	case CP0_TAGHI_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_taghi(state, cache);
	case CP0_RANDOM_REG:
	case CP0_BADVADDR_REG:
	case CP0_PREVID_REG:
		return TraceJITSettings.InterpretMTC0
		     ? mips32_emit_interpret(state, cache, &TJ_MTC0, 0)
		     : emit_mtc0_write_ignored(state, cache);
	default:
		return mips32_emit_interpret(state, cache, &TJ_MTC0, TJ_CHECK_STOP);
	}
}

enum TJEmitTraceResult mips32_emit_tlbr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_TLBR, 0);
}

enum TJEmitTraceResult mips32_emit_tlbwi(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_TLBWI, 0);
}

enum TJEmitTraceResult mips32_emit_tlbwr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_TLBWR, TJ_READS_PC);
}

enum TJEmitTraceResult mips32_emit_tlbp(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_TLBP, 0);
}

enum TJEmitTraceResult mips32_emit_eret(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_ERET, TJ_TRANSFERS_CONTROL);
}
