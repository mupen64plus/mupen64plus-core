/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rsp_core.c                                              *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#include "rsp_core.h"

#include <string.h>

#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/rcp/mi/mi_controller.h"
#include "device/rcp/rdp/rdp_core.h"
#include "device/rcp/ri/ri_controller.h"
#include "device/rdram/rdram.h"
#include "main/main.h"
#if defined(PROFILE)
#include "main/profile.h"
#endif
#include "plugin/plugin.h"
#include "api/callbacks.h"

static void do_sp_dma(struct rsp_core* sp, const struct sp_dma* dma)
{
    unsigned int i,j;

    unsigned int l = dma->length;

    unsigned int length = ((l & 0xfff) | 7) + 1;
    unsigned int count = ((l >> 12) & 0xff) + 1;
    unsigned int skip = ((l >> 20) & 0xfff);

    unsigned int memaddr = dma->memaddr & 0xff8;
    unsigned int dramaddr = dma->dramaddr & 0xfffff8;

    unsigned char *spmem = (unsigned char*)sp->mem + (dma->memaddr & 0x1000);
    unsigned char *dram = (unsigned char*)sp->ri->rdram->dram;

    if (dma->dir == SP_DMA_READ)
    {
        for(j=0; j<count; j++) {
            for(i=0; i<length; i++) {
                dram[(dramaddr^S8) & 0x7fffff] = spmem[(memaddr^S8) & 0xfff];
                memaddr++;
                dramaddr++;
            }
            if (dramaddr <= 0x800000)
                post_framebuffer_write(&sp->dp->fb, dramaddr - length, length);
            dramaddr+=skip;
        }

        sp->regs[SP_MEM_ADDR_REG] = memaddr & 0xfff;
        sp->regs[SP_DRAM_ADDR_REG] = dramaddr & 0xffffff;
        sp->regs[SP_RD_LEN_REG] = 0xff8;
    }
    else
    {
        for(j=0; j<count; j++) {
            if (dramaddr < 0x800000)
                pre_framebuffer_read(&sp->dp->fb, dramaddr);

            for(i=0; i<length; i++) {
                spmem[(memaddr & 0xfff)^S8] = dram[(dramaddr^S8) & 0x7fffff];
                memaddr++;
                dramaddr++;
            }
            dramaddr+=skip;
        }

        sp->regs[SP_MEM_ADDR_REG] = (memaddr & 0xfff) + (dma->memaddr & 0x1000);
        sp->regs[SP_DRAM_ADDR_REG] = dramaddr;

        sp->regs[SP_MEM_ADDR_REG] = memaddr & 0xfff;
        sp->regs[SP_DRAM_ADDR_REG] = dramaddr & 0xffffff;
        sp->regs[SP_RD_LEN_REG] = 0xff8;
        sp->regs[SP_WR_LEN_REG] = 0xff8;
    }

    /* schedule end of dma event */
    cp0_update_count(sp->mi->r4300);
    add_interrupt_event(&sp->mi->r4300->cp0, RSP_DMA_EVT, (count * length) / 8);
}

static void fifo_push(struct rsp_core* sp, uint32_t dir)
{
    if (sp->regs[SP_DMA_FULL_REG])
    {
        DebugMessage(M64MSG_WARNING, "RSP DMA attempted but FIFO queue already full.");
        return;
    }

    if (sp->regs[SP_DMA_BUSY_REG])
    {
        sp->fifo[1].dir = dir;
        sp->fifo[1].length = dir == SP_DMA_READ ? sp->regs[SP_WR_LEN_REG] : sp->regs[SP_RD_LEN_REG];
        sp->fifo[1].memaddr = sp->regs[SP_MEM_ADDR_REG];
        sp->fifo[1].dramaddr = sp->regs[SP_DRAM_ADDR_REG];
        sp->regs[SP_DMA_FULL_REG] = 1;
        sp->regs[SP_STATUS_REG] |= SP_STATUS_DMA_FULL;
    }
    else
    {
        sp->fifo[0].dir = dir;
        sp->fifo[0].length = dir == SP_DMA_READ ? sp->regs[SP_WR_LEN_REG] : sp->regs[SP_RD_LEN_REG];
        sp->fifo[0].memaddr = sp->regs[SP_MEM_ADDR_REG];
        sp->fifo[0].dramaddr = sp->regs[SP_DRAM_ADDR_REG];
        sp->regs[SP_DMA_BUSY_REG] = 1;
        sp->regs[SP_STATUS_REG] |= SP_STATUS_DMA_BUSY;

        do_sp_dma(sp, &sp->fifo[0]);
    }
}

static void fifo_pop(struct rsp_core* sp)
{
    if (sp->regs[SP_DMA_FULL_REG])
    {
        sp->fifo[0].dir = sp->fifo[1].dir;
        sp->fifo[0].length = sp->fifo[1].length;
        sp->fifo[0].memaddr = sp->fifo[1].memaddr;
        sp->fifo[0].dramaddr = sp->fifo[1].dramaddr;
        sp->regs[SP_DMA_FULL_REG] = 0;
        sp->regs[SP_STATUS_REG] &= ~SP_STATUS_DMA_FULL;

        do_sp_dma(sp, &sp->fifo[0]);
    }
    else
    {
        sp->regs[SP_DMA_BUSY_REG] = 0;
        sp->regs[SP_STATUS_REG] &= ~SP_STATUS_DMA_BUSY;
    }
}

static void update_sp_status(struct rsp_core* sp, uint32_t w)
{
    /* clear / set halt */
    if ((w & SP_CLR_HALT) && !(w & SP_SET_HALT))
    {
        sp->rsp_wait &= ~WAIT_HALTED;
        sp->regs[SP_STATUS_REG] &= ~SP_STATUS_HALT;
    }
    if ((w & SP_SET_HALT) && !(w & SP_CLR_HALT))
    {
        remove_event(&sp->mi->r4300->cp0.q, SP_INT);
        sp->rsp_status = 0;
        sp->first_run = 1;
        sp->rsp_wait |= WAIT_HALTED;
        sp->regs[SP_STATUS_REG] |= SP_STATUS_HALT;
    }

    /* clear broke */
    if (w & SP_CLR_BROKE) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_BROKE;

    /* clear SP interrupt */
    if ((w & SP_CLR_INTR) && !(w & SP_SET_INTR))
    {
        clear_rcp_interrupt(sp->mi, MI_INTR_SP);
    }
    /* set SP interrupt */
    if ((w & SP_SET_INTR) && !(w & SP_CLR_INTR))
    {
        signal_rcp_interrupt(sp->mi, MI_INTR_SP);
    }

    /* clear / set single step */
    if ((w & SP_CLR_SSTEP) && !(w & SP_SET_SSTEP)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SSTEP;
    if ((w & SP_SET_SSTEP) && !(w & SP_CLR_SSTEP)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SSTEP;

    /* clear / set interrupt on break */
    if ((w & SP_CLR_INTR_BREAK) && !(w & SP_SET_INTR_BREAK))
    {
        if (sp->rsp_wait & WAIT_PENDING_SP_INT_BROKE)
        {
            // If a game clears SP_SET_INTR_BREAK before the interrupt happens,
            // that means it would have been cleared before the BREAK command
            remove_event(&sp->mi->r4300->cp0.q, SP_INT);
            sp->rsp_wait &= ~WAIT_PENDING_SP_INT_BROKE;
            sp->regs[SP_STATUS_REG] = sp->rsp_status;
            sp->rsp_status = 0;
        }
        sp->regs[SP_STATUS_REG] &= ~SP_STATUS_INTR_BREAK;
    }
    if ((w & SP_SET_INTR_BREAK) && !(w & SP_CLR_INTR_BREAK)) sp->regs[SP_STATUS_REG] |= SP_STATUS_INTR_BREAK;

    /* clear / set signal 0 */
    if ((w & SP_CLR_SIG0) && !(w & SP_SET_SIG0)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG0;
    if ((w & SP_SET_SIG0) && !(w & SP_CLR_SIG0)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG0;

    /* clear / set signal 1 */
    if ((w & SP_CLR_SIG1) && !(w & SP_SET_SIG1)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG1;
    if ((w & SP_SET_SIG1) && !(w & SP_CLR_SIG1)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG1;

    /* clear / set signal 2 */
    if ((w & SP_CLR_SIG2) && !(w & SP_SET_SIG2)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG2;
    if ((w & SP_SET_SIG2) && !(w & SP_CLR_SIG2)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG2;

    /* clear / set signal 3 */
    if ((w & SP_CLR_SIG3) && !(w & SP_SET_SIG3)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG3;
    if ((w & SP_SET_SIG3) && !(w & SP_CLR_SIG3)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG3;

    /* clear / set signal 4 */
    if ((w & SP_CLR_SIG4) && !(w & SP_SET_SIG4)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG4;
    if ((w & SP_SET_SIG4) && !(w & SP_CLR_SIG4)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG4;

    /* clear / set signal 5 */
    if ((w & SP_CLR_SIG5) && !(w & SP_SET_SIG5)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG5;
    if ((w & SP_SET_SIG5) && !(w & SP_CLR_SIG5)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG5;

    /* clear / set signal 6 */
    if ((w & SP_CLR_SIG6) && !(w & SP_SET_SIG6)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG6;
    if ((w & SP_SET_SIG6) && !(w & SP_CLR_SIG6)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG6;

    /* clear / set signal 7 */
    if ((w & SP_CLR_SIG7) && !(w & SP_SET_SIG7)) sp->regs[SP_STATUS_REG] &= ~SP_STATUS_SIG7;
    if ((w & SP_SET_SIG7) && !(w & SP_CLR_SIG7)) sp->regs[SP_STATUS_REG] |= SP_STATUS_SIG7;

    do_SP_Task(sp);
}

void init_rsp(struct rsp_core* sp,
              uint32_t* sp_mem,
              struct mi_controller* mi,
              struct rdp_core* dp,
              struct ri_controller* ri)
{
    sp->mem = sp_mem;
    sp->mi = mi;
    sp->dp = dp;
    sp->ri = ri;
}

void poweron_rsp(struct rsp_core* sp)
{
    memset(sp->mem, 0, SP_MEM_SIZE);
    memset(sp->regs, 0, SP_REGS_COUNT*sizeof(uint32_t));
    memset(sp->regs2, 0, SP_REGS2_COUNT*sizeof(uint32_t));
    memset(sp->fifo, 0, SP_DMA_FIFO_SIZE*sizeof(struct sp_dma));

    sp->rsp_status = 0;
    sp->first_run = 1;
    sp->rsp_wait = 0;
    sp->mi->r4300->cp0.interrupt_unsafe_state &= ~INTR_UNSAFE_RSP;
    sp->regs[SP_STATUS_REG] = 1;
    sp->regs[SP_RD_LEN_REG] = 0xff8;
    sp->regs[SP_WR_LEN_REG] = 0xff8;
}


void read_rsp_mem(void* opaque, uint32_t address, uint32_t* value)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t addr = rsp_mem_address(address);

    *value = sp->mem[addr];
}

void write_rsp_mem(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t addr = rsp_mem_address(address);

    masked_write(&sp->mem[addr], value, mask);
}


void read_rsp_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t reg = rsp_reg(address);

    *value = sp->regs[reg];

    if (reg == SP_SEMAPHORE_REG)
    {
        sp->regs[SP_SEMAPHORE_REG] = 1;
    }
}

void write_rsp_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t reg = rsp_reg(address);

    switch(reg)
    {
    case SP_STATUS_REG:
        update_sp_status(sp, value & mask);
    case SP_DMA_FULL_REG:
    case SP_DMA_BUSY_REG:
        return;
    }

    masked_write(&sp->regs[reg], value, mask);

    switch(reg)
    {
    case SP_MEM_ADDR_REG:
        sp->regs[SP_MEM_ADDR_REG] &= 0x1ff8;
        break;
    case SP_DRAM_ADDR_REG:
        sp->regs[SP_DRAM_ADDR_REG] &= 0xfffff8;
        break;
    case SP_RD_LEN_REG:
        fifo_push(sp, SP_DMA_WRITE);
        break;
    case SP_WR_LEN_REG:
        fifo_push(sp, SP_DMA_READ);
        break;
    case SP_SEMAPHORE_REG:
        sp->regs[SP_SEMAPHORE_REG] = 0;
        break;
    }
}


void read_rsp_regs2(void* opaque, uint32_t address, uint32_t* value)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t reg = rsp_reg2(address);

    if (reg < SP_REGS2_COUNT)
        *value = sp->regs2[reg];

    if (reg == SP_PC_REG)
        *value &= 0xffc;
}

void write_rsp_regs2(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    uint32_t reg = rsp_reg2(address);

    if (reg == SP_PC_REG)
    {
        masked_write(&sp->regs2[SP_PC_REG], value & 0xffc, mask);
        return;
    }

    if (reg < SP_REGS2_COUNT)
        masked_write(&sp->regs2[reg], value, mask);
}

void do_SP_Task(struct rsp_core* sp)
{
    if (sp->rsp_wait)
        return;
    if (get_event(&sp->mi->r4300->cp0.q, RSP_TSK_EVT))
        return;

    uint32_t saved_status = sp->regs[SP_STATUS_REG];
    uint32_t sp_bit_set = sp->mi->regs[MI_INTR_REG] & MI_INTR_SP;
    uint32_t dp_bit_set = sp->mi->regs[MI_INTR_REG] & MI_INTR_DP;

    unprotect_framebuffers(&sp->dp->fb);
    uint32_t rsp_cycles = rsp.doRspCycles(sp->first_run) / 2;

    if (sp->mi->regs[MI_INTR_REG] & MI_INTR_DP && !dp_bit_set)
    {
        sp->mi->regs[MI_INTR_REG] &= ~MI_INTR_DP;
        sp->rsp_wait |= WAIT_PENDING_DP_SYNC;
        if (sp->dp->dpc_regs[DPC_STATUS_REG] & DPC_STATUS_FREEZE) {
            sp->dp->do_on_unfreeze |= DELAY_DP_INT;
        } else {
            cp0_update_count(sp->mi->r4300);
            add_interrupt_event(&sp->mi->r4300->cp0, DP_INT, rsp_cycles + sp->dp->dpc_regs[DPC_CLOCK_REG]);
        }
    }

    sp->rsp_status = sp->regs[SP_STATUS_REG];
    if ((sp->regs[SP_STATUS_REG] & SP_STATUS_HALT) == 0)
    {
        add_interrupt_event(&sp->mi->r4300->cp0, RSP_TSK_EVT, rsp_cycles);
        sp->first_run = 0;
    }
    else
    {
        sp->rsp_wait |= WAIT_HALTED;
        sp->first_run = 1;
    }
    if ((sp->regs[SP_STATUS_REG] & SP_STATUS_BROKE) && (sp->regs[SP_STATUS_REG] & SP_STATUS_INTR_BREAK))
    {
        sp->rsp_wait |= WAIT_PENDING_SP_INT_BROKE;
        cp0_update_count(sp->mi->r4300);
        add_interrupt_event(&sp->mi->r4300->cp0, SP_INT, rsp_cycles);
        sp->regs[SP_STATUS_REG] = saved_status;
    }
    else if (sp->mi->regs[MI_INTR_REG] & MI_INTR_SP && !sp_bit_set)
    {
        sp->rsp_wait |= WAIT_PENDING_SP_INT;
        cp0_update_count(sp->mi->r4300);
        add_interrupt_event(&sp->mi->r4300->cp0, SP_INT, rsp_cycles);
    }
    sp->mi->r4300->cp0.interrupt_unsafe_state |= INTR_UNSAFE_RSP;
    if (!sp_bit_set)
        sp->mi->regs[MI_INTR_REG] &= ~MI_INTR_SP;

    protect_framebuffers(&sp->dp->fb);
}

void rsp_interrupt_event(void* opaque)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;

    sp->regs[SP_STATUS_REG] = sp->rsp_status;
    sp->rsp_status = 0;
    sp->mi->r4300->cp0.interrupt_unsafe_state &= ~INTR_UNSAFE_RSP;
    raise_rcp_interrupt(sp->mi, MI_INTR_SP);

    clear_rsp_wait(sp, WAIT_PENDING_SP_INT | WAIT_PENDING_SP_INT_BROKE);
}

void rsp_end_of_dma_event(void* opaque)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;
    fifo_pop(sp);
}

void rsp_task_event(void* opaque)
{
    struct rsp_core* sp = (struct rsp_core*)opaque;

    do_SP_Task(sp);
}

void clear_rsp_wait(struct rsp_core* sp, uint32_t value)
{
    sp->rsp_wait &= ~value;

    do_SP_Task(sp);
}
