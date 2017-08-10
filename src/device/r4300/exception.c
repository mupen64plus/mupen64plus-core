/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - exception.c                                             *
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

#include <assert.h>
#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/r4300/exception.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "device/r4300/tlb.h"

void poweron_exception(struct cp0* cp0)
{
    cp0->current_exception = NULL;
    cp0->skipped_exception = NULL;
    cp0->exception_level = 0;
}

void add_exception_to_list(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);

    if(r4300->cp0.current_exception == NULL)
    {
        r4300->cp0.current_exception = (struct exception_infos*) malloc(sizeof(struct exception_infos));
        r4300->cp0.current_exception->next = NULL;
        r4300->cp0.current_exception->previous = NULL;
    }
    else
    {
        struct exception_infos *previous_exception = r4300->cp0.current_exception;
        r4300->cp0.current_exception->next = (struct exception_infos*) malloc(sizeof(struct exception_infos));
        r4300->cp0.current_exception = r4300->cp0.current_exception->next;
        r4300->cp0.current_exception->next = NULL;
        r4300->cp0.current_exception->previous = previous_exception;
    }

    r4300->cp0.current_exception->EPC = cp0_regs[CP0_EPC_REG];
    r4300->cp0.current_exception->fgr64 = ((cp0_regs[CP0_STATUS_REG] & UINT32_C(0x04000000)) != 0) ? 1 : 0;

    if(r4300->cp0.current_exception->fgr64 == 1)
    {
        // Switch to 32-bit mode
        shuffle_fpr_data(&r4300->cp1, cp0_regs[CP0_STATUS_REG], 0);
        set_fpr_pointers(&r4300->cp1, 0);
        cp0_regs[CP0_STATUS_REG] &= ~0x04000000;
    }
    r4300->cp0.exception_level++;
    assert((r4300->cp0.exception_level < 10) && (r4300->cp0.exception_level >= 0));
}

void remove_exception_from_list(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);
    struct exception_infos *curr = r4300->cp0.current_exception;

    while(curr)
    {
        if(curr->EPC == cp0_regs[CP0_EPC_REG])
        {
            if((curr->fgr64 == 1) && ((cp0_regs[CP0_STATUS_REG] & UINT32_C(0x04000000)) == 0))
            {
                // Switch to 64-bit mode
                shuffle_fpr_data(&r4300->cp1, cp0_regs[CP0_STATUS_REG], 0x04000000);
                set_fpr_pointers(&r4300->cp1, 0x04000000);
                cp0_regs[CP0_STATUS_REG] |= 0x04000000;
            }

            struct exception_infos *previous_exception = curr->previous;
            struct exception_infos *next_exception = curr->next;
            free(curr);

            if(next_exception)
            {
                assert(next_exception == r4300->cp0.skipped_exception);
                assert(next_exception->next == NULL);
                free(next_exception);
                r4300->cp0.exception_level--;
            }

            r4300->cp0.current_exception = previous_exception;
            if(r4300->cp0.current_exception)
                r4300->cp0.current_exception->next = NULL;

            r4300->cp0.exception_level--;
            break;
        }
        curr = curr->previous;
    }
}

void check_exception_list(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);
    struct exception_infos *curr = r4300->cp0.current_exception;

    while(curr)
    {
        if(curr->EPC == cp0_regs[CP0_EPC_REG])
        {
            if(curr != r4300->cp0.current_exception)
                r4300->cp0.skipped_exception = curr->next;
            break;
        }
        curr = curr->previous;
    }
}

void clear_exception_list(struct r4300_core* r4300)
{
    while(r4300->cp0.current_exception)
    {
        struct exception_infos *previous_exception = r4300->cp0.current_exception->previous;
        free(r4300->cp0.current_exception);
        r4300->cp0.current_exception = previous_exception;
    }
    r4300->cp0.exception_level = 0;
}

void TLB_refill_exception(struct r4300_core* r4300, uint32_t address, int w)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);
    int usual_handler = 0, i;

    if (r4300->emumode != EMUMODE_DYNAREC && w != 2) {
        cp0_update_count(r4300);
    }

    cp0_regs[CP0_CAUSE_REG] = (w == 1)
        ? CP0_CAUSE_EXCCODE_TLBS
        : CP0_CAUSE_EXCCODE_TLBL;

    cp0_regs[CP0_BADVADDR_REG] = address;
    cp0_regs[CP0_CONTEXT_REG] = (cp0_regs[CP0_CONTEXT_REG] & UINT32_C(0xFF80000F))
        | ((address >> 9) & UINT32_C(0x007FFFF0));
    cp0_regs[CP0_ENTRYHI_REG] = address & UINT32_C(0xFFFFE000);

    if (cp0_regs[CP0_STATUS_REG] & CP0_STATUS_EXL)
    {
        generic_jump_to(r4300, UINT32_C(0x80000180));


        if (r4300->delay_slot == 1 || r4300->delay_slot == 3) {
            cp0_regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
        }
        else {
            cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
        }
    }
    else
    {
        if (r4300->emumode != EMUMODE_PURE_INTERPRETER)
        {
            cp0_regs[CP0_EPC_REG] = (w != 2)
                ? *r4300_pc(r4300)
                : address;
        }
        else {
            cp0_regs[CP0_EPC_REG] = *r4300_pc(r4300);
        }

        cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
        cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_EXL;

        if (address >= UINT32_C(0x80000000) && address < UINT32_C(0xc0000000)) {
            usual_handler = 1;
        }

        for (i = 0; i < 32; i++)
        {
            if (/*r4300->cp0.tlb.entries[i].v_even &&*/ address >= r4300->cp0.tlb.entries[i].start_even &&
                    address <= r4300->cp0.tlb.entries[i].end_even) {
                usual_handler = 1;
            }
            if (/*r4300->cp0.tlb.entries[i].v_odd &&*/ address >= r4300->cp0.tlb.entries[i].start_odd &&
                    address <= r4300->cp0.tlb.entries[i].end_odd) {
                usual_handler = 1;
            }
        }

        generic_jump_to(r4300, (usual_handler)
                ? UINT32_C(0x80000180)
                : UINT32_C(0x80000000));
    }

    if (r4300->delay_slot == 1 || r4300->delay_slot == 3)
    {
        cp0_regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
        cp0_regs[CP0_EPC_REG] -= 4;
    }
    else
    {
        cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
    }
    if (w != 2) {
        cp0_regs[CP0_EPC_REG] -= 4;
    }

    r4300->cp0.last_addr = *r4300_pc(r4300);

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        dyna_jump();
        if (!r4300->dyna_interp) { r4300->delay_slot = 0; }
    }

    if (r4300->emumode != EMUMODE_DYNAREC || r4300->dyna_interp)
    {
        r4300->dyna_interp = 0;
        if (r4300->delay_slot)
        {
            r4300->skip_jump = *r4300_pc(r4300);
            *r4300_cp0_next_interrupt(&r4300->cp0) = 0;
        }
    }
}

void exception_general(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);

    cp0_update_count(r4300);
    cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_EXL;

    cp0_regs[CP0_EPC_REG] = *r4300_pc(r4300);

    if (r4300->delay_slot == 1 || r4300->delay_slot == 3)
    {
        cp0_regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
        cp0_regs[CP0_EPC_REG] -= 4;
    }
    else
    {
        cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
    }

    generic_jump_to(r4300, UINT32_C(0x80000180));

    r4300->cp0.last_addr = *r4300_pc(r4300);

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        dyna_jump();
        if (!r4300->dyna_interp) { r4300->delay_slot = 0; }
    }
    if (r4300->emumode != EMUMODE_DYNAREC || r4300->dyna_interp)
    {
        r4300->dyna_interp = 0;
        if (r4300->delay_slot)
        {
            r4300->skip_jump = *r4300_pc(r4300);
            *r4300_cp0_next_interrupt(&r4300->cp0) = 0;
        }
    }
}

