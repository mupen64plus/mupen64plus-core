/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pure_interp.c                                           *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#include <stdlib.h>
#include <math.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/debugger.h"
#include "memory/memory.h"
#include "main/rom.h"
#include "osal/preproc.h"

#include "r4300.h"
#include "exception.h"
#include "macros.h"
#include "interupt.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/debugger.h"
#endif

unsigned int interp_addr;
unsigned int op;
static int skip;

static void prefetch(void);

static void (*interp_ops[64])(void);

#define PCADDR interp_addr
#define ADD_TO_PC(x) interp_addr += x*4;
#define DECLARE_INSTRUCTION(name) static void name(void)
#define DECLARE_JUMP(name, destination, condition, link, likely, cop1) \
   static void name(void) \
   { \
      const int take_jump = (condition); \
      const unsigned int jump_target = (destination); \
      long long int *link_register = (link); \
      if (cop1 && check_cop1_unusable()) return; \
      if (take_jump && jump_target == interp_addr && probe_nop(interp_addr+4)) \
      { \
         update_count(); \
         skip = next_interupt - Count; \
         if (skip > 3)  \
         { \
            Count += (skip & 0xFFFFFFFC); \
            return; \
         } \
      } \
      if (!likely || take_jump) \
      { \
        interp_addr += 4; \
        delay_slot=1; \
        prefetch(); \
        interp_ops[((op >> 26) & 0x3F)](); \
        update_count(); \
        delay_slot=0; \
        if (take_jump && !skip_jump) \
        { \
          if (link_register != &reg[0]) \
          { \
              *link_register=interp_addr; \
              sign_extended(*link_register); \
          } \
          interp_addr = jump_target; \
        } \
      } \
      else \
      { \
         interp_addr += 8; \
         update_count(); \
      } \
      last_addr = interp_addr; \
      if (next_interupt <= Count) gen_interupt(); \
   }
#define CHECK_MEMORY(x)
#define CHECK_R0_WRITE(r) { if (r == &reg[0]) { interp_addr+=4; return; } }

#include "interpreter.def"

static void (*interp_special[64])(void) =
{
   SLL , NI   , SRL , SRA , SLLV   , NI    , SRLV  , SRAV  ,
   JR  , JALR , NI  , NI  , SYSCALL, NI    , NI    , SYNC  ,
   MFHI, MTHI , MFLO, MTLO, DSLLV  , NI    , DSRLV , DSRAV ,
   MULT, MULTU, DIV , DIVU, DMULT  , DMULTU, DDIV  , DDIVU ,
   ADD , ADDU , SUB , SUBU, AND    , OR    , XOR   , NOR   ,
   NI  , NI   , SLT , SLTU, DADD   , DADDU , DSUB  , DSUBU ,
   NI  , NI   , NI  , NI  , TEQ    , NI    , NI    , NI    ,
   DSLL, NI   , DSRL, DSRA, DSLL32 , NI    , DSRL32, DSRA32
};

static void (*interp_regimm[32])(void) =
{
   BLTZ  , BGEZ  , BLTZL  , BGEZL  , NI, NI, NI, NI,
   NI    , NI    , NI     , NI     , NI, NI, NI, NI,
   BLTZAL, BGEZAL, BLTZALL, BGEZALL, NI, NI, NI, NI,
   NI    , NI    , NI     , NI     , NI, NI, NI, NI
};

static void (*interp_tlb[64])(void) =
{
   NI  , TLBR, TLBWI, NI, NI, NI, TLBWR, NI,
   TLBP, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   ERET, NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI,
   NI  , NI  , NI   , NI, NI, NI, NI   , NI
};

static void TLB(void)
{
   interp_tlb[(op & 0x3F)]();
}

static void (*interp_cop0[32])(void) =
{
   MFC0, NI, NI, NI, MTC0, NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI,
   TLB , NI, NI, NI, NI  , NI, NI, NI,
   NI  , NI, NI, NI, NI  , NI, NI, NI
};

static void (*interp_cop1_bc[4])(void) =
{
   BC1F , BC1T,
   BC1FL, BC1TL
};

static void (*interp_cop1_s[64])(void) =
{
ADD_S    ,SUB_S    ,MUL_S   ,DIV_S    ,SQRT_S   ,ABS_S    ,MOV_S   ,NEG_S    ,
ROUND_L_S,TRUNC_L_S,CEIL_L_S,FLOOR_L_S,ROUND_W_S,TRUNC_W_S,CEIL_W_S,FLOOR_W_S,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,CVT_D_S  ,NI      ,NI       ,CVT_W_S  ,CVT_L_S  ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
C_F_S    ,C_UN_S   ,C_EQ_S  ,C_UEQ_S  ,C_OLT_S  ,C_ULT_S  ,C_OLE_S ,C_ULE_S  ,
C_SF_S   ,C_NGLE_S ,C_SEQ_S ,C_NGL_S  ,C_LT_S   ,C_NGE_S  ,C_LE_S  ,C_NGT_S
};

static void (*interp_cop1_d[64])(void) =
{
ADD_D    ,SUB_D    ,MUL_D   ,DIV_D    ,SQRT_D   ,ABS_D    ,MOV_D   ,NEG_D    ,
ROUND_L_D,TRUNC_L_D,CEIL_L_D,FLOOR_L_D,ROUND_W_D,TRUNC_W_D,CEIL_W_D,FLOOR_W_D,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
CVT_S_D  ,NI       ,NI      ,NI       ,CVT_W_D  ,CVT_L_D  ,NI      ,NI       ,
NI       ,NI       ,NI      ,NI       ,NI       ,NI       ,NI      ,NI       ,
C_F_D    ,C_UN_D   ,C_EQ_D  ,C_UEQ_D  ,C_OLT_D  ,C_ULT_D  ,C_OLE_D ,C_ULE_D  ,
C_SF_D   ,C_NGLE_D ,C_SEQ_D ,C_NGL_D  ,C_LT_D   ,C_NGE_D  ,C_LE_D  ,C_NGT_D
};

static void (*interp_cop1_w[64])(void) =
{
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   CVT_S_W, CVT_D_W, NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI
};

static void (*interp_cop1_l[64])(void) =
{
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   CVT_S_L, CVT_D_L, NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI,
   NI     , NI     , NI, NI, NI, NI, NI, NI
};

static void BC(void)
{
   interp_cop1_bc[(op >> 16) & 3]();
}

static void S(void)
{
   interp_cop1_s[(op & 0x3F)]();
}

static void D(void)
{
   interp_cop1_d[(op & 0x3F)]();
}

static void W(void)
{
   interp_cop1_w[(op & 0x3F)]();
}

static void L(void)
{
   interp_cop1_l[(op & 0x3F)]();
}

static void (*interp_cop1[32])(void) =
{
   MFC1, DMFC1, CFC1, NI, MTC1, DMTC1, CTC1, NI,
   BC  , NI   , NI  , NI, NI  , NI   , NI  , NI,
   S   , D    , NI  , NI, W   , L    , NI  , NI,
   NI  , NI   , NI  , NI, NI  , NI   , NI  , NI
};

static void SPECIAL(void)
{
   interp_special[(op & 0x3F)]();
}

static void REGIMM(void)
{
   interp_regimm[((op >> 16) & 0x1F)]();
}

static void COP0(void)
{
   interp_cop0[((op >> 21) & 0x1F)]();
}

static void COP1(void)
{
   if (check_cop1_unusable()) return;
   interp_cop1[((op >> 21) & 0x1F)]();
}

static void (*interp_ops[64])(void) =
{
   SPECIAL, REGIMM, J   , JAL  , BEQ , BNE , BLEZ , BGTZ ,
   ADDI   , ADDIU , SLTI, SLTIU, ANDI, ORI , XORI , LUI  ,
   COP0   , COP1  , NI  , NI   , BEQL, BNEL, BLEZL, BGTZL,
   DADDI  , DADDIU, LDL , LDR  , NI  , NI  , NI   , NI   ,
   LB     , LH    , LWL , LW   , LBU , LHU , LWR  , LWU  ,
   SB     , SH    , SWL , SW   , SDL , SDR , SWR  , CACHE,
   LL     , LWC1  , NI  , NI   , NI  , LDC1, NI   , LD   ,
   SC     , SWC1  , NI  , NI   , NI  , SDC1, NI   , SD
};

static void prefetch(void)
{
   unsigned int *mem = fast_mem_access(interp_addr);
   if (mem != NULL)
   {
      op = *mem;
      prefetch_opcode(op);
   }
   else
   {
      DebugMessage(M64MSG_ERROR, "prefetch() execute address :%x", (int)interp_addr);
      stop=1;
   }
}

void pure_interpreter(void)
{
   interp_addr = 0xa4000040;
   stop=0;
   PC = (precomp_instr *) malloc(sizeof(precomp_instr));
   PC->addr = last_addr = interp_addr;

/*#ifdef DBG
         if (g_DebuggerActive)
           update_debugger(PC->addr);
#endif*/

   while (!stop)
   {
     prefetch();
#ifdef COMPARE_CORE
     CoreCompareCallback();
#endif
#ifdef DBG
     PC->addr = interp_addr;
     if (g_DebuggerActive) update_debugger(PC->addr);
#endif
     interp_ops[((op >> 26) & 0x3F)]();
   }
   PC->addr = interp_addr;
}
