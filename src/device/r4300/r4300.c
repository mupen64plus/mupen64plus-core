/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300.c                                                 *
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/debugger.h"
#include "api/m64p_types.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/interupt.h"
#include "device/r4300/new_dynarec/new_dynarec.h"
#include "device/r4300/ops.h"
#include "device/r4300/pure_interp.h"
#include "device/r4300/r4300.h"
#include "device/r4300/r4300_core.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "device/r4300/tlb.h"
#include "main/main.h"
#include "main/rom.h"

#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif

#if defined(COUNT_INSTR)
#include "instr_counters.h"
#endif

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

void r4300_execute(void)
{
#if (defined(DYNAREC) && defined(PROFILE_R4300))
    unsigned int i;
#endif

    g_dev.r4300.current_instruction_table = cached_interpreter_table;

    *r4300_stop() = 0;
    g_rom_pause = 0;

    /* clear instruction counters */
#if defined(COUNT_INSTR)
    memset(instr_count, 0, 131*sizeof(instr_count[0]));
#endif

    /* XXX: might go to r4300_poweron / soft_reset ? */
    g_dev.r4300.cp0.last_addr = 0xa4000040;
    *r4300_cp0_next_interrupt() = 624999;
    init_interupt();

    if (g_dev.r4300.emumode == EMUMODE_PURE_INTERPRETER)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Pure Interpreter");
        g_dev.r4300.emumode = EMUMODE_PURE_INTERPRETER;
        pure_interpreter();
    }
#if defined(DYNAREC)
    else if (g_dev.r4300.emumode >= 2)
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Dynamic Recompiler");
        g_dev.r4300.emumode = EMUMODE_DYNAREC;
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
        g_dev.r4300.recomp.pfProfile = fopen("instructionaddrs.dat", "ab");
        for (i=0; i<0x100000; i++)
            if (g_dev.r4300.cached_interp.invalid_code[i] == 0 && g_dev.r4300.cached_interp.blocks[i] != NULL && g_dev.r4300.cached_interp.blocks[i]->code != NULL && g_dev.r4300.cached_interp.blocks[i]->block != NULL)
            {
                unsigned char *x86addr;
                int mipsop;
                // store final code length for this block
                mipsop = -1; /* -1 == end of x86 code block */
                x86addr = g_dev.r4300.cached_interp.blocks[i]->code + g_dev.r4300.cached_interp.blocks[i]->code_length;
                if (fwrite(&mipsop, 1, 4, g_dev.r4300.recomp.pfProfile) != 4 ||
                    fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile) != sizeof(char *))
                    DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
            }
        fclose(g_dev.r4300.recomp.pfProfile);
        g_dev.r4300.recomp.pfProfile = NULL;
#endif
        free_blocks();
    }
#endif
    else /* if (g_dev.r4300.emumode == EMUMODE_INTERPRETER) */
    {
        DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Cached Interpreter");
        g_dev.r4300.emumode = EMUMODE_INTERPRETER;
        init_blocks();
        jump_to(UINT32_C(0xa4000040));

        /* Prevent segfault on failed jump_to */
        if (!g_dev.r4300.cached_interp.actual->block)
            return;

        g_dev.r4300.cp0.last_addr = *r4300_pc();
        while (!*r4300_stop())
        {
#ifdef COMPARE_CORE
            if ((*r4300_pc_struct())->ops == cached_interpreter_table.FIN_BLOCK && ((*r4300_pc_struct())->addr < 0x80000000 || (*r4300_pc_struct())->addr >= 0xc0000000))
                virtual_to_physical_address((*r4300_pc_struct())->addr, 2);
            CoreCompareCallback();
#endif
#ifdef DBG
            if (g_DebuggerActive) update_debugger((*r4300_pc_struct())->addr);
#endif
            (*r4300_pc_struct())->ops();
        }

        free_blocks();
    }

    DebugMessage(M64MSG_INFO, "R4300 emulator finished.");

    /* print instruction counts */
#if defined(COUNT_INSTR)
    if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
        instr_counters_print();
#endif
}
