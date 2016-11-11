/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - vi_controller.c                                         *
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

#include "vi_controller.h"

#include <string.h>

#include "api/m64p_types.h"
#include "main/main.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/r4300_core.h"

/* XXX: timing hacks */
enum { NTSC_VERTICAL_RESOLUTION = 525 };

unsigned int vi_clock_from_tv_standard(m64p_system_type tv_standard)
{
    switch(tv_standard)
    {
    case SYSTEM_PAL:
        return 49656530;
    case SYSTEM_MPAL:
        return 48628316;
    case SYSTEM_NTSC:
    default:
        return 48681812;
    }
}

unsigned int vi_expected_refresh_rate_from_tv_standard(m64p_system_type tv_standard)
{
    switch (tv_standard)
    {
    case SYSTEM_PAL:
    case SYSTEM_MPAL:
        return 50;

    case SYSTEM_NTSC:
    default:
        return 60;
    }
}

void init_vi(struct vi_controller* vi, unsigned int clock, unsigned int expected_refresh_rate,
             unsigned int count_per_scanline, unsigned int alternate_timing,
             struct r4300_core* r4300)
{
    vi->clock = clock;
    vi->expected_refresh_rate = expected_refresh_rate;
    vi->count_per_scanline = count_per_scanline;
    vi->alternate_timing = alternate_timing;
    vi->r4300 = r4300;
}

void poweron_vi(struct vi_controller* vi)
{
    memset(vi->regs, 0, VI_REGS_COUNT*sizeof(uint32_t));
    vi->field = 0;
    vi->delay = vi->next_vi = 5000;
}


int read_vi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct vi_controller* vi = (struct vi_controller*)opaque;
    uint32_t reg = vi_reg(address);
    const uint32_t* cp0_regs = r4300_cp0_regs();

    if (reg == VI_CURRENT_REG)
    {
        /* XXX: update current line number */
        cp0_update_count();
        if (vi->alternate_timing)
            vi->regs[VI_CURRENT_REG] = (vi->delay - (vi->next_vi - cp0_regs[CP0_COUNT_REG])) % (NTSC_VERTICAL_RESOLUTION + 1);
        else
            vi->regs[VI_CURRENT_REG] = (vi->delay - (vi->next_vi - cp0_regs[CP0_COUNT_REG])) / vi->count_per_scanline;

        /* update current field */
        vi->regs[VI_CURRENT_REG] = (vi->regs[VI_CURRENT_REG] & (~1)) | vi->field;
    }

    *value = vi->regs[reg];

    return 0;
}

int write_vi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct vi_controller* vi = (struct vi_controller*)opaque;
    uint32_t reg = vi_reg(address);

    switch(reg)
    {
    case VI_STATUS_REG:
        if ((vi->regs[VI_STATUS_REG] & mask) != (value & mask))
        {
            masked_write(&vi->regs[VI_STATUS_REG], value, mask);
            gfx.viStatusChanged();
        }
        return 0;

    case VI_WIDTH_REG:
        if ((vi->regs[VI_WIDTH_REG] & mask) != (value & mask))
        {
            masked_write(&vi->regs[VI_WIDTH_REG], value, mask);
            gfx.viWidthChanged();
        }
        return 0;

    case VI_CURRENT_REG:
        clear_rcp_interrupt(vi->r4300, MI_INTR_VI);
        return 0;
    }

    masked_write(&vi->regs[reg], value, mask);

    return 0;
}

void vi_vertical_interrupt_event(struct vi_controller* vi)
{
    gfx.updateScreen();

    /* allow main module to do things on VI event */
    new_vi();

    /* toggle vi field if in interlaced mode */
    vi->field ^= (vi->regs[VI_STATUS_REG] >> 6) & 0x1;

    /* schedule next vertical interrupt */
    vi->delay = (vi->regs[VI_V_SYNC_REG] == 0)
            ? 500000
            : (vi->regs[VI_V_SYNC_REG] + 1) * vi->count_per_scanline;

    vi->next_vi += vi->delay;

    add_interupt_event_count(VI_INT, vi->next_vi);

    /* trigger interrupt */
    raise_rcp_interrupt(vi->r4300, MI_INTR_VI);
}

