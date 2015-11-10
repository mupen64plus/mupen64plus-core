/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-jit.h                                              *
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

#ifndef M64P_TRACE_JIT_MIPS_JIT_H
#define M64P_TRACE_JIT_MIPS_JIT_H

#include <stdbool.h>
#include <stdint.h>

enum TJEmitTraceResult {
	TJ_SUCCESS,
	TJ_MEMORY_ERROR,
	TJ_FAILURE
};

/* A trace function that calls the recompiler for TJ_PC.addr whenever it is
 * run. */
extern void TJ_NOT_CODE(void);

/* Does the same as TJ_NOT_CODE, but marks a trace as having been other code
 * at the same N64 memory location in the past. */
extern void TJ_FORMERLY_CODE(void);

/* A function that can be called from within emitted code in order to set up
 * a jump to the next trace.
 *
 * It first tries to get the address of existing native code for the address
 * given. If there is none, it tries to generate the code. If that fails, it
 * displays an error message and returns NULL. */
extern void* GetOrMakeTraceAt(uint32_t pc);

/* Settings that suppress optimisations. This structure is designed so that
 *
 *   memset(&<struct TJSettings>, 1, sizeof(struct TJSettings));
 * gives the safest, but slowest, execution environment, and
 *
 *   memset(&<struct TJSettings>, 0, sizeof(struct TJSettings));
 * gives a fast, but possibly unsafe, execution environment. */
struct TJSettings {
	bool InterpretNI;
	bool InterpretRESERVED;
	bool InterpretJ;
	bool InterpretJ_IDLE;
	bool InterpretJAL;
	bool InterpretJAL_IDLE;
	bool InterpretBEQ;
	bool InterpretBEQ_IDLE;
	bool InterpretBNE;
	bool InterpretBNE_IDLE;
	bool InterpretBLEZ;
	bool InterpretBLEZ_IDLE;
	bool InterpretBGTZ;
	bool InterpretBGTZ_IDLE;
	bool InterpretADDI;
	bool InterpretADDIU;
	bool InterpretSLTI;
	bool InterpretSLTIU;
	bool InterpretANDI;
	bool InterpretORI;
	bool InterpretXORI;
	bool InterpretLUI;
	bool InterpretBEQL;
	bool InterpretBEQL_IDLE;
	bool InterpretBNEL;
	bool InterpretBNEL_IDLE;
	bool InterpretBLEZL;
	bool InterpretBLEZL_IDLE;
	bool InterpretBGTZL;
	bool InterpretBGTZL_IDLE;
	bool InterpretDADDI;
	bool InterpretDADDIU;
	bool InterpretLDL;
	bool InterpretLDR;
	bool InterpretLB;
	bool InterpretLH;
	bool InterpretLWL;
	bool InterpretLW;
	bool InterpretLBU;
	bool InterpretLHU;
	bool InterpretLWR;
	bool InterpretLWU;
	bool InterpretSB;
	bool InterpretSH;
	bool InterpretSWL;
	bool InterpretSW;
	bool InterpretSDL;
	bool InterpretSDR;
	bool InterpretSWR;
	bool InterpretCACHE;
	bool InterpretLL;
	bool InterpretLWC1;
	bool InterpretLDC1;
	bool InterpretLD;
	bool InterpretSC;
	bool InterpretSWC1;
	bool InterpretSDC1;
	bool InterpretSD;
	bool InterpretMFC0;
	bool InterpretMTC0;
	bool InterpretBC1F;
	bool InterpretBC1F_IDLE;
	bool InterpretBC1T;
	bool InterpretBC1T_IDLE;
	bool InterpretBC1FL;
	bool InterpretBC1FL_IDLE;
	bool InterpretBC1TL;
	bool InterpretBC1TL_IDLE;
	bool InterpretMFC1;
	bool InterpretDMFC1;
	bool InterpretCFC1;
	bool InterpretMTC1;
	bool InterpretDMTC1;
	bool InterpretCTC1;
	bool InterpretADD_D;
	bool InterpretSUB_D;
	bool InterpretMUL_D;
	bool InterpretDIV_D;
	bool InterpretSQRT_D;
	bool InterpretABS_D;
	bool InterpretMOV_D;
	bool InterpretNEG_D;
	bool InterpretROUND_L_D;
	bool InterpretTRUNC_L_D;
	bool InterpretCEIL_L_D;
	bool InterpretFLOOR_L_D;
	bool InterpretROUND_W_D;
	bool InterpretTRUNC_W_D;
	bool InterpretCEIL_W_D;
	bool InterpretFLOOR_W_D;
	bool InterpretCVT_S_D;
	bool InterpretCVT_W_D;
	bool InterpretCVT_L_D;
	bool InterpretC_F_D;
	bool InterpretC_UN_D;
	bool InterpretC_EQ_D;
	bool InterpretC_UEQ_D;
	bool InterpretC_OLT_D;
	bool InterpretC_ULT_D;
	bool InterpretC_OLE_D;
	bool InterpretC_ULE_D;
	bool InterpretC_SF_D;
	bool InterpretC_NGLE_D;
	bool InterpretC_SEQ_D;
	bool InterpretC_NGL_D;
	bool InterpretC_LT_D;
	bool InterpretC_NGE_D;
	bool InterpretC_LE_D;
	bool InterpretC_NGT_D;
	bool InterpretCVT_S_L;
	bool InterpretCVT_D_L;
	bool InterpretADD_S;
	bool InterpretSUB_S;
	bool InterpretMUL_S;
	bool InterpretDIV_S;
	bool InterpretSQRT_S;
	bool InterpretABS_S;
	bool InterpretMOV_S;
	bool InterpretNEG_S;
	bool InterpretROUND_L_S;
	bool InterpretTRUNC_L_S;
	bool InterpretCEIL_L_S;
	bool InterpretFLOOR_L_S;
	bool InterpretROUND_W_S;
	bool InterpretTRUNC_W_S;
	bool InterpretCEIL_W_S;
	bool InterpretFLOOR_W_S;
	bool InterpretCVT_D_S;
	bool InterpretCVT_W_S;
	bool InterpretCVT_L_S;
	bool InterpretC_F_S;
	bool InterpretC_UN_S;
	bool InterpretC_EQ_S;
	bool InterpretC_UEQ_S;
	bool InterpretC_OLT_S;
	bool InterpretC_ULT_S;
	bool InterpretC_OLE_S;
	bool InterpretC_ULE_S;
	bool InterpretC_SF_S;
	bool InterpretC_NGLE_S;
	bool InterpretC_SEQ_S;
	bool InterpretC_NGL_S;
	bool InterpretC_LT_S;
	bool InterpretC_NGE_S;
	bool InterpretC_LE_S;
	bool InterpretC_NGT_S;
	bool InterpretCVT_S_W;
	bool InterpretCVT_D_W;
	bool InterpretBLTZ;
	bool InterpretBLTZ_IDLE;
	bool InterpretBGEZ;
	bool InterpretBGEZ_IDLE;
	bool InterpretBLTZL;
	bool InterpretBLTZL_IDLE;
	bool InterpretBGEZL;
	bool InterpretBGEZL_IDLE;
	bool InterpretBLTZAL;
	bool InterpretBLTZAL_IDLE;
	bool InterpretBGEZAL;
	bool InterpretBGEZAL_IDLE;
	bool InterpretBLTZALL;
	bool InterpretBLTZALL_IDLE;
	bool InterpretBGEZALL;
	bool InterpretBGEZALL_IDLE;
	bool InterpretNOP;
	bool InterpretSLL;
	bool InterpretSRL;
	bool InterpretSRA;
	bool InterpretSLLV;
	bool InterpretSRLV;
	bool InterpretSRAV;
	bool InterpretJR;
	bool InterpretJALR;
	bool InterpretSYSCALL;
	bool InterpretSYNC;
	bool InterpretMFHI;
	bool InterpretMTHI;
	bool InterpretMFLO;
	bool InterpretMTLO;
	bool InterpretDSLLV;
	bool InterpretDSRLV;
	bool InterpretDSRAV;
	bool InterpretMULT;
	bool InterpretMULTU;
	bool InterpretDIV;
	bool InterpretDIVU;
	bool InterpretDMULT;
	bool InterpretDMULTU;
	bool InterpretDDIV;
	bool InterpretDDIVU;
	bool InterpretADD;
	bool InterpretADDU;
	bool InterpretSUB;
	bool InterpretSUBU;
	bool InterpretAND;
	bool InterpretOR;
	bool InterpretXOR;
	bool InterpretNOR;
	bool InterpretSLT;
	bool InterpretSLTU;
	bool InterpretDADD;
	bool InterpretDADDU;
	bool InterpretDSUB;
	bool InterpretDSUBU;
	bool InterpretTEQ;
	bool InterpretDSLL;
	bool InterpretDSRL;
	bool InterpretDSRA;
	bool InterpretDSLL32;
	bool InterpretDSRL32;
	bool InterpretDSRA32;
	bool InterpretTLBR;
	bool InterpretTLBWI;
	bool InterpretTLBWR;
	bool InterpretTLBP;
	bool InterpretERET;

	/* If true, SWC1 and SDC1 are considered able to modify code.
	 * If false, SWC1 and SDC1 are not considered able to modify code.
	 *
	 * SWC1 and SDC1 are memory store instructions. However, given that they
	 * are also floating-point instructions, they store the results of
	 * floating-point calculations to memory. It is possible that some games
	 * use MTC1 SWC1 to store a modified instruction to memory, or use
	 * DMTC1 SDC1 to store 2 modified instructions, but this should never
	 * happen in practice.
	 */
	bool Cop1CanModifyCode;

	/* If true, stores to memory via $29 are considered able to modify code.
	 * If false, stores to memory via $29 are not considered able to modify
	 * code.
	 *
	 * $29 is the stack pointer in the MIPS calling convention, and usually
	 * data on the stack is not executed. Nintendo 64 games obey this
	 * convention, even inside the bootloader. It is possible that some games
	 * set $29 to a different memory location and write new instructions to
	 * memory through it, but this should never happen in practice.
	 */
	bool StackCanModifyCode;

	/* If true, SB is considered able to modify code.
	 * If false, SB is not considered able to modify code.
	 *
	 * SB stores single bytes to memory. It is possible that some games use SB
	 * to rewrite single bytes within instructions; however, given that MIPS
	 * instructions are 32 bits wide and don't have operand fields aligned to
	 * 8 bits, except immediate instructions which have the immediate in their
	 * lower 16 bits, SB should not be used in practice to rewrite code.
	 */
	bool SBCanModifyCode;

	/* If true, SH is considered able to modify code.
	 * If false, SH is not considered able to modify code.
	 *
	 * SH stores half-words to memory. It is possible that some games use SH
	 * to rewrite the Imm16 field within instructions, but this should be
	 * quite rare.
	 */
	bool SHCanModifyCode;

	/* If true, SD is considered able to modify code.
	 * If false, SD is not considered able to modify code.
	 *
	 * SD stores double-words (64 bits) to memory. It is possible that some
	 * games use SD to write two instructions at once in a 64-bit memcpy-style
	 * function. It is also possible that two instructions are modified at
	 * once; however, there is no easy access to bits #63..32 in MIPS III, so
	 * that is more likely to be done via SW. Many games use 32-bit memcpy
	 * code, however, due to the 32-bit data bus on the Nintendo 64.
	 * Most games are going to be using SD to preserve the values of registers
	 * for exception handling and, in games with threads, context switching.
	 */
	bool SDCanModifyCode;
};

extern struct TJSettings TraceJITSettings;

#endif /* !M64P_TRACE_JIT_MIPS_JIT_H */
