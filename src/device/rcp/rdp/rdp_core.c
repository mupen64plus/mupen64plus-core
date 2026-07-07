/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rdp_core.c                                              *
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

#include "rdp_core.h"

#include <string.h>

#include "device/memory/memory.h"
#include "device/rcp/mi/mi_controller.h"
#include "device/rcp/rsp/rsp_core.h"
#include "device/r4300/r4300_core.h"
#include "plugin/plugin.h"

static void update_dpc_status(struct rdp_core* dp, uint32_t w)
{
    /* clear / set xbus_dmem_dma */
    if (w & DPC_CLR_XBUS_DMEM_DMA) dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_XBUS_DMEM_DMA;
    if (w & DPC_SET_XBUS_DMEM_DMA) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_XBUS_DMEM_DMA;

    /* clear / set freeze */
    if (w & DPC_CLR_FREEZE)
    {
        dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_FREEZE;

        if (dp->do_on_unfreeze & DELAY_DP_INT)
        {
            signal_rcp_interrupt(dp->mi, MI_INTR_DP);

            clear_rsp_wait(dp->sp, WAIT_PENDING_DP_SYNC);
        }
        if (dp->do_on_unfreeze & DELAY_UPDATESCREEN)
            gfx.updateScreen();
        dp->do_on_unfreeze = 0;
    }
    if (w & DPC_SET_FREEZE) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_FREEZE;

    /* clear / set flush */
    if (w & DPC_CLR_FLUSH) dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_FLUSH;
    if (w & DPC_SET_FLUSH) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_FLUSH;

    if (w & DPC_CLR_TMEM_CTR)
    {
        dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_TMEM_BUSY;
        dp->dpc_regs[DPC_TMEM_REG] = 0;
    }
    if (w & DPC_CLR_PIPE_CTR)
    {
        dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_PIPE_BUSY;
        dp->dpc_regs[DPC_PIPEBUSY_REG] = 0;
    }
    if (w & DPC_CLR_CMD_CTR)
    {
        dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_CMD_BUSY;
        dp->dpc_regs[DPC_BUFBUSY_REG] = 0;
    }

    /* clear clock counter */
    if (w & DPC_CLR_CLOCK_CTR) dp->dpc_regs[DPC_CLOCK_REG] = 0;
}


void init_rdp(struct rdp_core* dp,
              struct rsp_core* sp,
              struct mi_controller* mi,
              struct memory* mem,
              struct rdram* rdram,
              struct r4300_core* r4300)
{
    dp->sp = sp;
    dp->mi = mi;

    init_fb(&dp->fb, mem, rdram, r4300);
}

void poweron_rdp(struct rdp_core* dp)
{
    memset(dp->dpc_regs, 0, DPC_REGS_COUNT*sizeof(uint32_t));
    memset(dp->dps_regs, 0, DPS_REGS_COUNT*sizeof(uint32_t));
    dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_START_GCLK | DPC_STATUS_PIPE_BUSY | DPC_STATUS_CBUF_READY;

    dp->do_on_unfreeze = 0;

    poweron_fb(&dp->fb);
}


void read_dpc_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dpc_reg(address);

    *value = dp->dpc_regs[reg];
}

void write_dpc_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dpc_reg(address);

    switch(reg)
    {
    case DPC_STATUS_REG:
        update_dpc_status(dp, value & mask);
    case DPC_CURRENT_REG:
    case DPC_CLOCK_REG:
    case DPC_BUFBUSY_REG:
    case DPC_PIPEBUSY_REG:
    case DPC_TMEM_REG:
        return;
    }

    switch(reg)
    {
    case DPC_START_REG:
        if (!(dp->dpc_regs[DPC_STATUS_REG] & DPC_STATUS_START_VALID))
        {
            masked_write(&dp->dpc_regs[reg], value & UINT32_C(0xFFFFF8), mask);
        }
        dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_START_VALID;
        break;
    case DPC_END_REG:
        masked_write(&dp->dpc_regs[reg], value & UINT32_C(0xFFFFF8), mask);
        if (dp->dpc_regs[DPC_STATUS_REG] & DPC_STATUS_START_VALID)
        {
            dp->dpc_regs[DPC_CURRENT_REG] = dp->dpc_regs[DPC_START_REG];
            dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_START_VALID;
        }
        unprotect_framebuffers(&dp->fb);
        gfx.processRDPList();
        protect_framebuffers(&dp->fb);
        if (dp->mi->regs[MI_INTR_REG] & MI_INTR_DP)
        {
            dp->mi->regs[MI_INTR_REG] &= ~MI_INTR_DP;
            if (dp->dpc_regs[DPC_STATUS_REG] & DPC_STATUS_FREEZE) {
                dp->do_on_unfreeze |= DELAY_DP_INT;
            } else {
                add_interrupt_event(&dp->mi->r4300->cp0, DP_INT, dp->dpc_regs[DPC_CLOCK_REG]);
            }
        }
        break;
    default:
        masked_write(&dp->dpc_regs[reg], value, mask);
        break;
    }
}


void read_dps_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dps_reg(address);

    if (reg < DPS_REGS_COUNT)
    {
        *value = dp->dps_regs[reg];
    }
}

void write_dps_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dps_reg(address);

    if (reg < DPS_REGS_COUNT)
    {
        masked_write(&dp->dps_regs[reg], value, mask);
    }
}

void rdp_interrupt_event(void* opaque)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;

    raise_rcp_interrupt(dp->mi, MI_INTR_DP);
    
    clear_rsp_wait(dp->sp, WAIT_PENDING_DP_SYNC);
}

