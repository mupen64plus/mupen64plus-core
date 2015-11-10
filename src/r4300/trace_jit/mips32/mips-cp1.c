/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) Coprocessor 1 instructions           *
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

#include "mips-cp1.h"

#include "mips-common.h"
#include "mips-emit.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../fpu.h"
#include "../../r4300.h"

/* - - - HELPER FUNCTIONS - - - */

static void emit_compare_s(struct mips32_state* state, struct mips32_reg_cache* cache, uint_fast8_t fp_pred)
{
	uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
	uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
	        nft = mips32_alloc_float_in_32(state, cache, ft),
	        efcr_31 = mips32_alloc_int_in_32(state, cache, &g_state.regs.fcr_31),
	        nc1cond_bit = mips32_alloc_int_const(state, cache, FCR31_CMP_BIT),
	        nfcr_31 = mips32_alloc_int_temp(state, cache),
	        nt1 = mips32_alloc_int_temp(state, cache);
	mips32_alloc_int_out_32(state, cache, &g_state.regs.fcr_31);

	/* TODO Declare that the native FCR31 has the most recent condition
	 * bit */
	mips32_fp_compare(state, FP_FORMAT_S, fp_pred, nfs, nft);
	mips32_cfc1(state, nfcr_31, 31);
	/* Merge the condition bit of the native FCR31 into the Nintendo 64
	 * FCR31:
	 *    (N64 & ~FCR31_CMP_BIT) | (native & FCR31_CMP_BIT)
	 * == N64 ^ ((N64 ^ native) & FCR31_CMP_BIT)
	 * <http://graphics.stanford.edu/~seander/bithacks.html#MaskedMerge> */
	mips32_xor(state, nt1, efcr_31, nfcr_31);
	mips32_and(state, nt1, nt1, nc1cond_bit);
	mips32_xor(state, efcr_31, nt1, efcr_31);
}

static void emit_compare_d(struct mips32_state* state, struct mips32_reg_cache* cache, uint_fast8_t fp_pred)
{
	uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
	uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
	        nft = mips32_alloc_float_in_64(state, cache, ft),
	        efcr_31 = mips32_alloc_int_in_32(state, cache, &g_state.regs.fcr_31),
	        nc1cond_bit = mips32_alloc_int_const(state, cache, FCR31_CMP_BIT),
	        nfcr_31 = mips32_alloc_int_temp(state, cache),
	        nt1 = mips32_alloc_int_temp(state, cache);
	mips32_alloc_int_out_32(state, cache, &g_state.regs.fcr_31);

	/* TODO Declare that the native FCR31 has the most recent condition
	 * bit */
	mips32_fp_compare(state, FP_FORMAT_D, fp_pred, nfs, nft);
	mips32_cfc1(state, nfcr_31, 31);
	/* Merge the condition bit of the native FCR31 into the Nintendo 64
	 * FCR31:
	 *    (N64 & ~FCR31_CMP_BIT) | (native & FCR31_CMP_BIT)
	 * == N64 ^ ((N64 ^ native) & FCR31_CMP_BIT)
	 * <http://graphics.stanford.edu/~seander/bithacks.html#MaskedMerge> */
	mips32_xor(state, nt1, efcr_31, nfcr_31);
	mips32_and(state, nt1, nt1, nc1cond_bit);
	mips32_xor(state, efcr_31, nt1, efcr_31);
}

/* - - - PUBLIC FUNCTIONS - - - */

enum TJEmitTraceResult mips32_emit_mfc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMFC1) {
		return mips32_emit_interpret(state, cache, &TJ_MFC1, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), fs = FS_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MFC1");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
		mips32_mfc1(state, nt, nfs);
		mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dmfc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	/* DMFC1 is not recompiled because the native-side MIPS FPU is essentially
	 * incompatible with itself.
	 *
	 * On MIPS32, 64-bit integer registers don't exist. So two parts need to
	 * be downloaded from the native FPU.
	 *
	 * One way would be to use MFC1 MFC1 from an even/odd FPR pair. This works
	 * only if we know that the native FPU is in the 16 double registers mode.
	 *
	 * Another way, available since MIPS32R2, is to use MFC1 MFHC1 on an FPR.
	 * This works only if we know that the code will be run on MIPS32R2.
	 *
	 * TODO Satisfy DMFC1 as SDC1 LW LW via the stack's memory accessor area
	 */
	return mips32_emit_interpret(state, cache, &TJ_DMFC1, 0);
}

enum TJEmitTraceResult mips32_emit_cfc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_CFC1, 0);
}

enum TJEmitTraceResult mips32_emit_mtc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMTC1) {
		return mips32_emit_interpret(state, cache, &TJ_MTC1, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), fs = FS_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MTC1");
		uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]),
		        nfs = mips32_alloc_float_out_32(state, cache, fs);
		mips32_mtc1(state, nt, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_dmtc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	/* DMTC1 is not recompiled because the native-side MIPS FPU is essentially
	 * incompatible with itself.
	 *
	 * On MIPS32, 64-bit integer registers don't exist. So two parts need to
	 * be uploaded to the native FPU.
	 *
	 * One way would be to use MTC1 MTC1 from an even/odd FPR pair. This works
	 * only if we know that the native FPU is in the 16 double registers mode.
	 *
	 * Another way, available since MIPS32R2, is to use MTC1 MTHC1 on an FPR.
	 * This works only if we know that the code will be run on MIPS32R2.
	 *
	 * TODO Satisfy DMTC1 as SW SW LDC1 via the stack's memory accessor area
	 */
	return mips32_emit_interpret(state, cache, &TJ_DMTC1, 0);
}

enum TJEmitTraceResult mips32_emit_ctc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	state->rounding_set = false;
	return mips32_emit_interpret(state, cache, &TJ_CTC1, 0);
}

enum TJEmitTraceResult mips32_emit_add_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADD_S) {
		return mips32_emit_interpret(state, cache, &TJ_ADD_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ADD.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nft = mips32_alloc_float_in_32(state, cache, ft),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_add(state, FP_FORMAT_S, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sub_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSUB_S) {
		return mips32_emit_interpret(state, cache, &TJ_SUB_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SUB.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nft = mips32_alloc_float_in_32(state, cache, ft),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_sub(state, FP_FORMAT_S, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mul_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMUL_S) {
		return mips32_emit_interpret(state, cache, &TJ_MUL_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MUL.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nft = mips32_alloc_float_in_32(state, cache, ft),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_mul(state, FP_FORMAT_S, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_div_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDIV_S) {
		return mips32_emit_interpret(state, cache, &TJ_DIV_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DIV.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nft = mips32_alloc_float_in_32(state, cache, ft),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_div(state, FP_FORMAT_S, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sqrt_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSQRT_S) {
		return mips32_emit_interpret(state, cache, &TJ_SQRT_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SQRT.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_sqrt(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_abs_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretABS_S) {
		return mips32_emit_interpret(state, cache, &TJ_ABS_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ABS.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_abs(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mov_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMOV_S) {
		return mips32_emit_interpret(state, cache, &TJ_MOV_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MOV.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_mov(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_neg_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretNEG_S) {
		return mips32_emit_interpret(state, cache, &TJ_NEG_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "NEG.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_neg(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_round_l_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretROUND_L_S) {
		return mips32_emit_interpret(state, cache, &TJ_ROUND_L_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ROUND.L.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_round_l(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_trunc_l_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretTRUNC_L_S) {
		return mips32_emit_interpret(state, cache, &TJ_TRUNC_L_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "TRUNC.L.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_trunc_l(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ceil_l_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCEIL_L_S) {
		return mips32_emit_interpret(state, cache, &TJ_CEIL_L_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CEIL.L.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_ceil_l(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_floor_l_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretFLOOR_L_S) {
		return mips32_emit_interpret(state, cache, &TJ_FLOOR_L_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "FLOOR.L.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_floor_l(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_round_w_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretROUND_W_S) {
		return mips32_emit_interpret(state, cache, &TJ_ROUND_W_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ROUND.W.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_round_w(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_trunc_w_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretTRUNC_W_S) {
		return mips32_emit_interpret(state, cache, &TJ_TRUNC_W_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "TRUNC.W.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_trunc_w(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ceil_w_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCEIL_W_S) {
		return mips32_emit_interpret(state, cache, &TJ_CEIL_W_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CEIL.W.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_ceil_w(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_floor_w_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretFLOOR_W_S) {
		return mips32_emit_interpret(state, cache, &TJ_FLOOR_W_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "FLOOR.W.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_floor_w(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_d_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_D_S) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_D_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.D.S");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_cvt_d(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
	return mips32_emit_interpret(state, cache, &TJ_CVT_D_S, 0);
}

enum TJEmitTraceResult mips32_emit_cvt_w_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_W_S) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_W_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.W.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_cvt_w(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_l_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_L_S) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_L_S, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.L.S");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_cvt_l(state, FP_FORMAT_S, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_f_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_F_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_F_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.F.S");
		emit_compare_s(state, cache, 0);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_un_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_UN_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_UN_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.UN.S");
		emit_compare_s(state, cache, FP_PREDICATE_UN);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_eq_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_EQ_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_EQ_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.EQ.S");
		emit_compare_s(state, cache, FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ueq_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_UEQ_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_UEQ_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.UEQ.S");
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_olt_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_OLT_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_OLT_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.OLT.S");
		emit_compare_s(state, cache, FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ult_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_ULT_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_ULT_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.ULT.S");
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ole_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_OLE_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_OLE_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.OLE.S");
		emit_compare_s(state, cache, FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ule_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_ULE_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_ULE_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.ULE.S");
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_sf_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_SF_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_SF_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.SF.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, 0);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngle_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGLE_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGLE_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGLE.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_UN);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_seq_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_SEQ_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_SEQ_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.SEQ.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngl_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGL_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGL_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGL.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_lt_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_LT_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_LT_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.LT.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_nge_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGE_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGE_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGE.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_le_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_LE_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_LE_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.LE.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngt_s(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGT_S) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGT_S, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGT.S");
		/* Ignore quiet NaNs */
		emit_compare_s(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_add_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretADD_D) {
		return mips32_emit_interpret(state, cache, &TJ_ADD_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ADD.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nft = mips32_alloc_float_in_64(state, cache, ft),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_add(state, FP_FORMAT_D, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sub_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSUB_D) {
		return mips32_emit_interpret(state, cache, &TJ_SUB_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SUB.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nft = mips32_alloc_float_in_64(state, cache, ft),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_sub(state, FP_FORMAT_D, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mul_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMUL_D) {
		return mips32_emit_interpret(state, cache, &TJ_MUL_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MUL.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nft = mips32_alloc_float_in_64(state, cache, ft),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_mul(state, FP_FORMAT_D, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_div_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretDIV_D) {
		return mips32_emit_interpret(state, cache, &TJ_DIV_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), ft = FT_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "DIV.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nft = mips32_alloc_float_in_64(state, cache, ft),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_div(state, FP_FORMAT_D, nfd, nfs, nft);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sqrt_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSQRT_D) {
		return mips32_emit_interpret(state, cache, &TJ_SQRT_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "SQRT.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_sqrt(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_abs_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretABS_D) {
		return mips32_emit_interpret(state, cache, &TJ_ABS_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ABS.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_abs(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_mov_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretMOV_D) {
		return mips32_emit_interpret(state, cache, &TJ_MOV_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "MOV.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_mov(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_neg_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretNEG_D) {
		return mips32_emit_interpret(state, cache, &TJ_NEG_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "NEG.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_neg(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_round_l_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretROUND_L_D) {
		return mips32_emit_interpret(state, cache, &TJ_ROUND_L_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ROUND.L.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_round_l(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_trunc_l_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretTRUNC_L_D) {
		return mips32_emit_interpret(state, cache, &TJ_TRUNC_L_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "TRUNC.L.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_trunc_l(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ceil_l_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCEIL_L_D) {
		return mips32_emit_interpret(state, cache, &TJ_CEIL_L_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CEIL.L.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_ceil_l(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_floor_l_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretFLOOR_L_D) {
		return mips32_emit_interpret(state, cache, &TJ_FLOOR_L_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "FLOOR.L.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_floor_l(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_round_w_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretROUND_W_D) {
		return mips32_emit_interpret(state, cache, &TJ_ROUND_W_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "ROUND.W.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_round_w(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_trunc_w_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretTRUNC_W_D) {
		return mips32_emit_interpret(state, cache, &TJ_TRUNC_W_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "TRUNC.W.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_trunc_w(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ceil_w_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCEIL_W_D) {
		return mips32_emit_interpret(state, cache, &TJ_CEIL_W_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CEIL.W.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_ceil_w(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_floor_w_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretFLOOR_W_D) {
		return mips32_emit_interpret(state, cache, &TJ_FLOOR_W_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "FLOOR.W.D");
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_floor_w(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_s_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_S_D) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_S_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.S.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_cvt_s(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_w_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_W_D) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_W_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.W.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_cvt_w(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_l_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_L_D) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_L_D, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.L.D");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_cvt_l(state, FP_FORMAT_D, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_f_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_F_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_F_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.F.D");
		emit_compare_d(state, cache, 0);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_un_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_UN_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_UN_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.UN.D");
		emit_compare_d(state, cache, FP_PREDICATE_UN);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_eq_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_EQ_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_EQ_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.EQ.D");
		emit_compare_d(state, cache, FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ueq_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_UEQ_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_UEQ_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.UEQ.D");
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_olt_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_OLT_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_OLT_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.OLT.D");
		emit_compare_d(state, cache, FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ult_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_ULT_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_ULT_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.ULT.D");
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ole_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_OLE_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_OLE_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.OLE.D");
		emit_compare_d(state, cache, FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ule_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_ULE_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_ULE_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.ULE.D");
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_sf_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_SF_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_SF_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.SF.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, 0);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngle_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGLE_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGLE_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGLE.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_UN);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_seq_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_SEQ_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_SEQ_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.SEQ.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngl_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGL_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGL_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGL.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_lt_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_LT_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_LT_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.LT.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_nge_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGE_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGE_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGE.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_le_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_LE_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_LE_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.LE.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_c_ngt_d(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretC_NGT_D) {
		return mips32_emit_interpret(state, cache, &TJ_C_NGT_D, 0);
	} else {
		mips32_start_opcode(state, cache, "C.NGT.D");
		/* Ignore quiet NaNs */
		emit_compare_d(state, cache, FP_PREDICATE_UN | FP_PREDICATE_EQ | FP_PREDICATE_LT);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_s_w(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_S_W) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_S_W, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.S.W");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_cvt_s(state, FP_FORMAT_W, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_d_w(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_D_W) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_D_W, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.D.W");
		uint8_t nfs = mips32_alloc_float_in_32(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_cvt_d(state, FP_FORMAT_W, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_s_l(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_S_L) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_S_L, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.S.L");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_32(state, cache, fd);
		mips32_fp_cvt_s(state, FP_FORMAT_L, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_cvt_d_l(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCVT_D_L) {
		return mips32_emit_interpret(state, cache, &TJ_CVT_D_L, 0);
	} else {
		uint8_t fs = FS_OF(state->ops[0]), fd = FD_OF(state->ops[0]);
		mips32_start_opcode(state, cache, "CVT.D.L");
		mips32_ensure_rounding(state, cache);
		uint8_t nfs = mips32_alloc_float_in_64(state, cache, fs),
		        nfd = mips32_alloc_float_out_64(state, cache, fd);
		mips32_fp_cvt_d(state, FP_FORMAT_L, nfd, nfs);
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}
