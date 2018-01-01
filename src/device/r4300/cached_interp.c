/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cached_interp.c                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#include <stdint.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/debugger.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/exception.h"
#include "device/r4300/interrupt.h"
#include "device/r4300/macros.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomp.h"
#include "device/r4300/tlb.h"
#include "main/main.h"

#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif

// -----------------------------------------------------------
// Cached interpreter functions (and fallback for dynarec).
// -----------------------------------------------------------
#ifdef DBG
#define UPDATE_DEBUGGER() if (g_DebuggerActive) update_debugger(*r4300_pc(r4300))
#else
#define UPDATE_DEBUGGER() do { } while(0)
#endif

#define DECLARE_R4300 struct r4300_core* r4300 = &g_dev.r4300;
#define PCADDR *r4300_pc(r4300)
#define ADD_TO_PC(x) (*r4300_pc_struct(r4300)) += x;
#define DECLARE_INSTRUCTION(name) static void name(void)

#define DECLARE_JUMP(name, destination, condition, link, likely, cop1) \
   static void name(void) \
   { \
      DECLARE_R4300 \
      const int take_jump = (condition); \
      const uint32_t jump_target = (destination); \
      int64_t *link_register = (link); \
      if (cop1 && check_cop1_unusable(r4300)) return; \
      if (link_register != &r4300_regs(r4300)[0]) \
      { \
         *link_register = SE32(*r4300_pc(r4300) + 8); \
      } \
      if (!likely || take_jump) \
      { \
         (*r4300_pc_struct(r4300))++; \
         r4300->delay_slot=1; \
         UPDATE_DEBUGGER(); \
         (*r4300_pc_struct(r4300))->ops(); \
         cp0_update_count(r4300); \
         r4300->delay_slot=0; \
         if (take_jump && !r4300->skip_jump) \
         { \
            (*r4300_pc_struct(r4300))=r4300->cached_interp.actual->block+((jump_target-r4300->cached_interp.actual->start)>>2); \
         } \
      } \
      else \
      { \
         (*r4300_pc_struct(r4300)) += 2; \
         cp0_update_count(r4300); \
      } \
      r4300->cp0.last_addr = *r4300_pc(r4300); \
      if (*r4300_cp0_next_interrupt(&r4300->cp0) <= r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]) gen_interrupt(r4300); \
   } \
   static void name##_OUT(void) \
   { \
      DECLARE_R4300 \
      const int take_jump = (condition); \
      const uint32_t jump_target = (destination); \
      int64_t *link_register = (link); \
      if (cop1 && check_cop1_unusable(r4300)) return; \
      if (link_register != &r4300_regs(r4300)[0]) \
      { \
         *link_register = SE32(*r4300_pc(r4300) + 8); \
      } \
      if (!likely || take_jump) \
      { \
         (*r4300_pc_struct(r4300))++; \
         r4300->delay_slot=1; \
         UPDATE_DEBUGGER(); \
         (*r4300_pc_struct(r4300))->ops(); \
         cp0_update_count(r4300); \
         r4300->delay_slot=0; \
         if (take_jump && !r4300->skip_jump) \
         { \
            cached_interpreter_dynarec_jump_to(r4300, jump_target); \
         } \
      } \
      else \
      { \
         (*r4300_pc_struct(r4300)) += 2; \
         cp0_update_count(r4300); \
      } \
      r4300->cp0.last_addr = *r4300_pc(r4300); \
      if (*r4300_cp0_next_interrupt(&r4300->cp0) <= r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]) gen_interrupt(r4300); \
   } \
   static void name##_IDLE(void) \
   { \
      DECLARE_R4300 \
      uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0); \
      const int take_jump = (condition); \
      int skip; \
      if (cop1 && check_cop1_unusable(r4300)) return; \
      if (take_jump) \
      { \
         cp0_update_count(r4300); \
         skip = *r4300_cp0_next_interrupt(&r4300->cp0) - cp0_regs[CP0_COUNT_REG]; \
         if (skip > 3) cp0_regs[CP0_COUNT_REG] += (skip & UINT32_C(0xFFFFFFFC)); \
         else name(); \
      } \
      else name(); \
   }

// two functions are defined from the macros above but never used
// these prototype declarations will prevent a warning
#if defined(__GNUC__)
  static void JR_IDLE(void) __attribute__((used));
  static void JALR_IDLE(void) __attribute__((used));
#endif

#include "mips_instructions.def"

// -----------------------------------------------------------
// Flow control 'fake' instructions
// -----------------------------------------------------------
static void FIN_BLOCK(void)
{
   DECLARE_R4300
   if (!r4300->delay_slot)
     {
    cached_interpreter_dynarec_jump_to(r4300, ((*r4300_pc_struct(r4300))-1)->addr+4);
/*#ifdef DBG
            if (g_DebuggerActive) update_debugger(*r4300_pc(r4300));
#endif
Used by dynarec only, check should be unnecessary
*/
    (*r4300_pc_struct(r4300))->ops();
    if (r4300->emumode == EMUMODE_DYNAREC) dyna_jump();
     }
   else
     {
    struct precomp_block *blk = r4300->cached_interp.actual;
    struct precomp_instr *inst = (*r4300_pc_struct(r4300));
    cached_interpreter_dynarec_jump_to(r4300, ((*r4300_pc_struct(r4300))-1)->addr+4);

/*#ifdef DBG
            if (g_DebuggerActive) update_debugger(*r4300_pc(r4300));
#endif
Used by dynarec only, check should be unnecessary
*/
    if (!r4300->skip_jump)
      {
         (*r4300_pc_struct(r4300))->ops();
         r4300->cached_interp.actual = blk;
         (*r4300_pc_struct(r4300)) = inst+1;
      }
    else
      (*r4300_pc_struct(r4300))->ops();

    if (r4300->emumode == EMUMODE_DYNAREC) dyna_jump();
     }
}

static void NOTCOMPILED(void)
{
   DECLARE_R4300
   uint32_t *mem = fast_mem_access(r4300, r4300->cached_interp.blocks[*r4300_pc(r4300)>>12]->start);
#ifdef DBG
   DebugMessage(M64MSG_INFO, "NOTCOMPILED: addr = %x ops = %lx", *r4300_pc(r4300), (long) (*r4300_pc_struct(r4300))->ops);
#endif

   if (mem != NULL)
      recompile_block(r4300, mem, r4300->cached_interp.blocks[*r4300_pc(r4300) >> 12], *r4300_pc(r4300));
   else
      DebugMessage(M64MSG_ERROR, "not compiled exception");

/*#ifdef DBG
            if (g_DebuggerActive) update_debugger(*r4300_pc(r4300));
#endif
The preceeding update_debugger SHOULD be unnecessary since it should have been
called before NOTCOMPILED would have been executed
*/
   (*r4300_pc_struct(r4300))->ops();
   if (r4300->emumode == EMUMODE_DYNAREC)
     dyna_jump();
}

static void NOTCOMPILED2(void)
{
   NOTCOMPILED();
}

// -----------------------------------------------------------
// Cached interpreter instruction table
// -----------------------------------------------------------
const struct cpu_instruction_table cached_interpreter_table = {
   LB,
   LBU,
   LH,
   LHU,
   LW,
   LWL,
   LWR,
   SB,
   SH,
   SW,
   SWL,
   SWR,

   LD,
   LDL,
   LDR,
   LL,
   LWU,
   SC,
   SD,
   SDL,
   SDR,
   SYNC,

   ADDI,
   ADDIU,
   SLTI,
   SLTIU,
   ANDI,
   ORI,
   XORI,
   LUI,

   DADDI,
   DADDIU,

   ADD,
   ADDU,
   SUB,
   SUBU,
   SLT,
   SLTU,
   AND,
   OR,
   XOR,
   NOR,

   DADD,
   DADDU,
   DSUB,
   DSUBU,

   MULT,
   MULTU,
   DIV,
   DIVU,
   MFHI,
   MTHI,
   MFLO,
   MTLO,

   DMULT,
   DMULTU,
   DDIV,
   DDIVU,

   J,
   J_OUT,
   J_IDLE,
   JAL,
   JAL_OUT,
   JAL_IDLE,
   // Use the _OUT versions of JR and JALR, since we don't know
   // until runtime if they're going to jump inside or outside the block
   JR_OUT,
   JALR_OUT,
   BEQ,
   BEQ_OUT,
   BEQ_IDLE,
   BNE,
   BNE_OUT,
   BNE_IDLE,
   BLEZ,
   BLEZ_OUT,
   BLEZ_IDLE,
   BGTZ,
   BGTZ_OUT,
   BGTZ_IDLE,
   BLTZ,
   BLTZ_OUT,
   BLTZ_IDLE,
   BGEZ,
   BGEZ_OUT,
   BGEZ_IDLE,
   BLTZAL,
   BLTZAL_OUT,
   BLTZAL_IDLE,
   BGEZAL,
   BGEZAL_OUT,
   BGEZAL_IDLE,

   BEQL,
   BEQL_OUT,
   BEQL_IDLE,
   BNEL,
   BNEL_OUT,
   BNEL_IDLE,
   BLEZL,
   BLEZL_OUT,
   BLEZL_IDLE,
   BGTZL,
   BGTZL_OUT,
   BGTZL_IDLE,
   BLTZL,
   BLTZL_OUT,
   BLTZL_IDLE,
   BGEZL,
   BGEZL_OUT,
   BGEZL_IDLE,
   BLTZALL,
   BLTZALL_OUT,
   BLTZALL_IDLE,
   BGEZALL,
   BGEZALL_OUT,
   BGEZALL_IDLE,
   BC1TL,
   BC1TL_OUT,
   BC1TL_IDLE,
   BC1FL,
   BC1FL_OUT,
   BC1FL_IDLE,

   SLL,
   SRL,
   SRA,
   SLLV,
   SRLV,
   SRAV,

   DSLL,
   DSRL,
   DSRA,
   DSLLV,
   DSRLV,
   DSRAV,
   DSLL32,
   DSRL32,
   DSRA32,

   MTC0,
   MFC0,

   TLBR,
   TLBWI,
   TLBWR,
   TLBP,
   CACHE,
   ERET,

   LWC1,
   SWC1,
   MTC1,
   MFC1,
   CTC1,
   CFC1,
   BC1T,
   BC1T_OUT,
   BC1T_IDLE,
   BC1F,
   BC1F_OUT,
   BC1F_IDLE,

   DMFC1,
   DMTC1,
   LDC1,
   SDC1,

   CVT_S_D,
   CVT_S_W,
   CVT_S_L,
   CVT_D_S,
   CVT_D_W,
   CVT_D_L,
   CVT_W_S,
   CVT_W_D,
   CVT_L_S,
   CVT_L_D,

   ROUND_W_S,
   ROUND_W_D,
   ROUND_L_S,
   ROUND_L_D,

   TRUNC_W_S,
   TRUNC_W_D,
   TRUNC_L_S,
   TRUNC_L_D,

   CEIL_W_S,
   CEIL_W_D,
   CEIL_L_S,
   CEIL_L_D,

   FLOOR_W_S,
   FLOOR_W_D,
   FLOOR_L_S,
   FLOOR_L_D,

   ADD_S,
   ADD_D,

   SUB_S,
   SUB_D,

   MUL_S,
   MUL_D,

   DIV_S,
   DIV_D,

   ABS_S,
   ABS_D,

   MOV_S,
   MOV_D,

   NEG_S,
   NEG_D,

   SQRT_S,
   SQRT_D,

   C_F_S,
   C_F_D,
   C_UN_S,
   C_UN_D,
   C_EQ_S,
   C_EQ_D,
   C_UEQ_S,
   C_UEQ_D,
   C_OLT_S,
   C_OLT_D,
   C_ULT_S,
   C_ULT_D,
   C_OLE_S,
   C_OLE_D,
   C_ULE_S,
   C_ULE_D,
   C_SF_S,
   C_SF_D,
   C_NGLE_S,
   C_NGLE_D,
   C_SEQ_S,
   C_SEQ_D,
   C_NGL_S,
   C_NGL_D,
   C_LT_S,
   C_LT_D,
   C_NGE_S,
   C_NGE_D,
   C_LE_S,
   C_LE_D,
   C_NGT_S,
   C_NGT_D,

   SYSCALL,

   TEQ,

   NOP,
   RESERVED,
   NI,

   FIN_BLOCK,
   NOTCOMPILED,
   NOTCOMPILED2
};

static uint32_t update_invalid_addr(struct r4300_core* r4300, uint32_t addr)
{
    if (addr >= 0x80000000 && addr < 0xc0000000)
    {
        if (r4300->cached_interp.invalid_code[addr>>12]) {
            r4300->cached_interp.invalid_code[(addr^0x20000000)>>12] = 1;
        }
        if (r4300->cached_interp.invalid_code[(addr^0x20000000)>>12]) {
            r4300->cached_interp.invalid_code[addr>>12] = 1;
        }
        return addr;
    }
    else
    {
        uint32_t paddr = virtual_to_physical_address(r4300, addr, 2);
        if (paddr)
        {
            uint32_t beg_paddr = paddr - (addr - (addr & ~0xfff));

            update_invalid_addr(r4300, paddr);

            if (r4300->cached_interp.invalid_code[(beg_paddr+0x000)>>12]) {
                r4300->cached_interp.invalid_code[addr>>12] = 1;
            }
            if (r4300->cached_interp.invalid_code[(beg_paddr+0xffc)>>12]) {
                r4300->cached_interp.invalid_code[addr>>12] = 1;
            }
            if (r4300->cached_interp.invalid_code[addr>>12]) {
                r4300->cached_interp.invalid_code[(beg_paddr+0x000)>>12] = 1;
            }
            if (r4300->cached_interp.invalid_code[addr>>12]) {
                r4300->cached_interp.invalid_code[(beg_paddr+0xffc)>>12] = 1;
            }
        }
        return paddr;
    }
}

void cached_interpreter_dynarec_jump_to(struct r4300_core* r4300, uint32_t address)
{
    struct cached_interp* const cinterp = &r4300->cached_interp;
    struct precomp_block** b;

    if (r4300->skip_jump) {
        return;
    }

    if (!update_invalid_addr(r4300, address)) {
        return;
    }

    b = &cinterp->blocks[address >> 12];

    cinterp->actual = *b;

    /* setup new block if invalid */
    if (cinterp->invalid_code[address >> 12])
    {
        if (!*b)
        {
            *b = (struct precomp_block*)malloc(sizeof(struct precomp_block));
            cinterp->actual = *b;
            (*b)->code = NULL;
            (*b)->block = NULL;
            (*b)->jumps_table = NULL;
            (*b)->riprel_table = NULL;
        }

        (*b)->start = (address & ~0xfff);
        (*b)->end = (address & ~0xfff) + 0x1000;

        init_block(r4300, *b);
    }

    /* set new PC */
    (*r4300_pc_struct(r4300)) = cinterp->actual->block + ((address - cinterp->actual->start) >> 2);

    /* set new PC for dynarec (eg set "return_address")*/
    if (r4300->emumode == EMUMODE_DYNAREC) {
        dyna_jump();
    }
}


void init_blocks(struct r4300_core* r4300)
{
    size_t i;
    struct cached_interp* cinterp = &r4300->cached_interp;

    for (i = 0; i < 0x100000; ++i)
    {
        cinterp->invalid_code[i] = 1;
        cinterp->blocks[i] = NULL;
    }
}

void free_blocks(struct r4300_core* r4300)
{
    size_t i;
    struct cached_interp* cinterp = &r4300->cached_interp;

    for (i = 0; i < 0x100000; ++i)
    {
        if (cinterp->blocks[i])
        {
            free_block(r4300, cinterp->blocks[i]);
            free(cinterp->blocks[i]);
            cinterp->blocks[i] = NULL;
        }
    }
}

void invalidate_cached_code_hacktarux(struct r4300_core* r4300, uint32_t address, size_t size)
{
    size_t i;
    uint32_t addr;
    uint32_t addr_max;

    if (size == 0)
    {
        /* invalidate everthing */
        memset(r4300->cached_interp.invalid_code, 1, 0x100000);
    }
    else
    {
        /* invalidate blocks (if necessary) */
        addr_max = address+size;

        for(addr = address; addr < addr_max; addr += 4)
        {
            i = (addr >> 12);

            if (r4300->cached_interp.invalid_code[i] == 0)
            {
                if (r4300->cached_interp.blocks[i] == NULL
                || r4300->cached_interp.blocks[i]->block[(addr & 0xfff) / 4].ops != r4300->current_instruction_table.NOTCOMPILED)
                {
                    r4300->cached_interp.invalid_code[i] = 1;
                    /* go directly to next i */
                    addr &= ~0xfff;
                    addr |= 0xffc;
                }
            }
            else
            {
                /* go directly to next i */
                addr &= ~0xfff;
                addr |= 0xffc;
            }
        }
    }
}

void run_cached_interpreter(struct r4300_core* r4300)
{
    while (!*r4300_stop(r4300))
    {
#ifdef COMPARE_CORE
        if ((*r4300_pc_struct(r4300))->ops == cached_interpreter_table.FIN_BLOCK && ((*r4300_pc_struct(r4300))->addr < 0x80000000 || (*r4300_pc_struct(r4300))->addr >= 0xc0000000))
            virtual_to_physical_address(r4300, (*r4300_pc_struct(r4300))->addr, 2);
        CoreCompareCallback();
#endif
#ifdef DBG
        if (g_DebuggerActive) update_debugger((*r4300_pc_struct(r4300))->addr);
#endif
        (*r4300_pc_struct(r4300))->ops();
    }
}
