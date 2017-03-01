/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300_core.c                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "r4300_core.h"
#include "cached_interp.h"
#if defined(COUNT_INSTR)
#include "instr_counters.h"
#endif
#include "mi_controller.h"
#include "new_dynarec/new_dynarec.h" /* for NEW_DYNAREC_ARM */
#include "pure_interp.h"
#include "recomp.h"

#include "api/callbacks.h"
#include "api/debugger.h"
#include "api/m64p_types.h"
#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif
#include "main/main.h"


#include <string.h>

extern int64_t g_dev_r4300_regs[32];
extern int64_t g_dev_r4300_hi;
extern int64_t g_dev_r4300_lo;
extern struct precomp_instr* g_dev_r4300_pc;
extern int g_dev_r4300_stop;

void init_r4300(struct r4300_core* r4300, unsigned int emumode, unsigned int count_per_op, int no_compiled_jump)
{
    r4300->emumode = emumode;
    init_cp0(&r4300->cp0, count_per_op);

    r4300->recomp.no_compiled_jump = no_compiled_jump;
}

void poweron_r4300(struct r4300_core* r4300)
{
    /* clear registers */
    memset(r4300_regs(), 0, 32*sizeof(int64_t));
    *r4300_mult_hi() = 0;
    *r4300_mult_lo() = 0;
    r4300->llbit = 0;

    *r4300_pc_struct() = NULL;
    r4300->delay_slot = 0;
    r4300->local_rs = 0;
    r4300->skip_jump = 0;
    r4300->dyna_interp = 0;
    //r4300->current_instruction_table;
    r4300->reset_hard_job = 0;

    r4300->jumps_table = NULL;
    r4300->jumps_number = 0;
    r4300->max_jumps_number = 0;
    r4300->jump_start8 = 0;
    r4300->jump_start32 = 0;
#if defined(__x86_64__)
    r4300->riprel_table = NULL;
    r4300->riprel_number = 0;
    r4300->max_riprel_number = 0;
#endif

#if defined(__x86_64__)
    r4300->save_rsp = 0;
    r4300->save_rip = 0;
#else
    r4300->save_ebp = 0;
    r4300->save_ebx = 0;
    r4300->save_esi = 0;
    r4300->save_edi = 0;
    r4300->save_esp = 0;
    r4300->save_eip = 0;
#endif

    /* recomp init */
    r4300->recomp.fast_memory = 1;
    r4300->recomp.delay_slot_compiled = 0;

    r4300->branch_taken = 0;

    /* setup CP0 registers */
    poweron_cp0(&r4300->cp0);

    /* setup CP1 registers */
    poweron_cp1(&r4300->cp1);

    /* setup mi */
    poweron_mi(&r4300->mi);
}


#if !defined(NO_ASM)
static void dynarec_setup_code(void)
{
   // The dynarec jumps here after we call dyna_start and it prepares
   // Here we need to prepare the initial code block and jump to it
   jump_to(UINT32_C(0xa4000040));

   // Prevent segfault on failed jump_to
   if (!g_dev.r4300.cached_interp.actual->block || !g_dev.r4300.cached_interp.actual->code)
      dyna_stop();
}
#endif

void run_r4300(struct r4300_core* r4300)
{
    r4300->current_instruction_table = cached_interpreter_table;

    *r4300_stop() = 0;
    g_rom_pause = 0;

    /* clear instruction counters */
#if defined(COUNT_INSTR)
    memset(instr_count, 0, 131*sizeof(instr_count[0]));
#endif

    /* XXX: might go to r4300_poweron / soft_reset ? */
    r4300->cp0.last_addr = 0xa4000040;
    *r4300_cp0_next_interrupt() = 624999;
    init_interupt();

    if (r4300->emumode == EMUMODE_PURE_INTERPRETER)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Pure Interpreter");
        r4300->emumode = EMUMODE_PURE_INTERPRETER;
        run_pure_interpreter(r4300);
    }
#if defined(DYNAREC)
    else if (r4300->emumode >= 2)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Dynamic Recompiler");
        r4300->emumode = EMUMODE_DYNAREC;
        init_blocks();

#ifdef NEW_DYNAREC
        new_dynarec_init();
        new_dyna_start();
        new_dynarec_cleanup();
#else
        dyna_start(dynarec_setup_code);
        (*r4300_pc_struct())++;
#endif
#if defined(PROFILE_R4300)
        profile_write_end_of_code_blocks(r4300);
#endif
        free_blocks();
    }
#endif
    else /* if (r4300->emumode == EMUMODE_INTERPRETER) */
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Cached Interpreter");
        r4300->emumode = EMUMODE_INTERPRETER;
        init_blocks();
        jump_to(UINT32_C(0xa4000040));

        /* Prevent segfault on failed jump_to */
        if (!r4300->cached_interp.actual->block)
            return;

        r4300->cp0.last_addr = *r4300_pc();

        run_cached_interpreter(r4300);

        free_blocks();
    }

    DebugMessage(M64MSG_INFO, "R4300 emulator finished.");

    /* print instruction counts */
#if defined(COUNT_INSTR)
    if (r4300->emumode == EMUMODE_DYNAREC)
        instr_counters_print();
#endif
}

int64_t* r4300_regs(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return g_dev.r4300.regs;
#else
    return g_dev_r4300_regs;
#endif
}

int64_t* r4300_mult_hi(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.hi;
#else
    return &g_dev_r4300_hi;
#endif
}

int64_t* r4300_mult_lo(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.lo;
#else
    return &g_dev_r4300_lo;
#endif
}

unsigned int* r4300_llbit(void)
{
    return &g_dev.r4300.llbit;
}

uint32_t* r4300_pc(void)
{
#ifdef NEW_DYNAREC
    return (g_dev.r4300.emumode == EMUMODE_DYNAREC)
        ? (uint32_t*)&pcaddr
        : &(*r4300_pc_struct())->addr;
#else
    return &(*r4300_pc_struct())->addr;
#endif
}

struct precomp_instr** r4300_pc_struct(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.pc;
#else
    return &g_dev_r4300_pc;
#endif
}

int* r4300_stop(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.stop;
#else
    return &g_dev_r4300_stop;
#endif
}

unsigned int get_r4300_emumode(struct r4300_core* r4300)
{
    return r4300->emumode;
}



void invalidate_r4300_cached_code(uint32_t address, size_t size)
{
    if (g_dev.r4300.emumode != EMUMODE_PURE_INTERPRETER)
    {
#ifdef NEW_DYNAREC
        if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
        {
            invalidate_cached_code_new_dynarec(address, size);
        }
        else
#endif
        {
            invalidate_cached_code_hacktarux(address, size);
        }
    }
}


void generic_jump_to(uint32_t address)
{
   if (g_dev.r4300.emumode == EMUMODE_PURE_INTERPRETER)
      *r4300_pc() = address;
   else {
#ifdef NEW_DYNAREC
      if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
         g_dev.r4300.cp0.last_addr = pcaddr;
      else
         jump_to(address);
#else
      jump_to(address);
#endif
   }
}


/* XXX: not really a good interface but it gets the job done... */
void savestates_load_set_pc(uint32_t pc)
{
#ifdef NEW_DYNAREC
    if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
    {
        pcaddr = pc;
        pending_exception = 1;
        invalidate_all_pages();
    }
    else
#endif
    {
        generic_jump_to(pc);
        invalidate_r4300_cached_code(0,0);
    }
}
