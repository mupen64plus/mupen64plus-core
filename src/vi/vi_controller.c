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

#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/r4300_core.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"

#include <string.h>

void connect_vi(struct vi_controller* vi,
                struct r4300_core* r4300)
{
    vi->r4300 = r4300;
}

void init_vi(struct vi_controller* vi)
{
    memset(vi->regs, 0, VI_REGS_COUNT*sizeof(uint32_t));
}


int read_vi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct vi_controller* vi = (struct vi_controller*)opaque;
    uint32_t reg = vi_reg(address);

    if (reg == VI_CURRENT_REG)
    {
        update_count();
        vi->regs[VI_CURRENT_REG] = (vi->delay - (next_vi - g_cp0_regs[CP0_COUNT_REG]))/1500;
        vi->regs[VI_CURRENT_REG] = (vi->regs[VI_CURRENT_REG] & (~1)) | vi_field;
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
        vi->r4300->mi.regs[MI_INTR_REG] &= ~0x8;
        check_interupt();
        return 0;
    }

    masked_write(&vi->regs[reg], value, mask);

    return 0;
}



