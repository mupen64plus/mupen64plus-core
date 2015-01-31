/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - ai_controller.c                                         *
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

#include "ai_controller.h"

#include "main/rom.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/r4300_core.h"
#include "r4300/interupt.h"
#include "vi/vi_controller.h"

#include <string.h>


static uint32_t get_remaining_dma_length(struct ai_controller* ai)
{
    unsigned int ai_event;
    unsigned int ai_delay;

    if (ai->fifo[0].delay == 0)
        return 0;

    update_count();
    ai_event = get_event(AI_INT);
    if (ai_event == 0)
        return 0;

    ai_delay = ai_event - g_cp0_regs[CP0_COUNT_REG];

    if (ai_delay >= 0x80000000)
        return 0;

    return (uint64_t)ai_delay * ai->fifo[0].length / ai->fifo[0].delay;
}

static unsigned int get_dma_duration(struct ai_controller* ai)
{
    unsigned int samples_per_sec = ROM_PARAMS.aidacrate / (1 + ai->regs[AI_DACRATE_REG]);

    return ((uint64_t)ai->regs[AI_LEN_REG]*ai->vi->delay*ROM_PARAMS.vilimit)
        / (4 * samples_per_sec);
}

void connect_ai(struct ai_controller* ai,
                struct r4300_core* r4300,
                struct vi_controller* vi)
{
    ai->r4300 = r4300;
    ai->vi = vi;
}

void init_ai(struct ai_controller* ai)
{
    memset(ai->regs, 0, AI_REGS_COUNT*sizeof(uint32_t));
    memset(ai->fifo, 0, 2*sizeof(struct ai_dma));
}


int read_ai_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct ai_controller* ai = (struct ai_controller*)opaque;
    uint32_t reg = ai_reg(address);

    if (reg == AI_LEN_REG)
    {
        *value = get_remaining_dma_length(ai);
    }
    else
    {
        *value = ai->regs[reg];
    }

    return 0;
}

int write_ai_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct ai_controller* ai = (struct ai_controller*)opaque;
    uint32_t reg = ai_reg(address);
    unsigned int delay;

    switch (reg)
    {
    case AI_LEN_REG:
        masked_write(&ai->regs[AI_LEN_REG], value, mask);
        audio.aiLenChanged();

        delay = get_dma_duration(ai);
        if (ai->regs[AI_STATUS_REG] & 0x40000000) // busy
        {
            ai->fifo[1].delay = delay;
            ai->fifo[1].length = ai->regs[AI_LEN_REG];
            ai->regs[AI_STATUS_REG] |= 0x80000000;
        }
        else
        {
            ai->fifo[0].delay = delay;
            ai->fifo[0].length = ai->regs[AI_LEN_REG];
            update_count();
            add_interupt_event(AI_INT, delay);
            ai->regs[AI_STATUS_REG] |= 0x40000000;
        }
        return 0;

    case AI_STATUS_REG:
        clear_rcp_interrupt(ai->r4300, MI_INTR_AI);
        return 0;

    case AI_DACRATE_REG:
        if ((ai->regs[AI_DACRATE_REG] & mask) != (value & mask))
        {
            masked_write(&ai->regs[AI_DACRATE_REG], value, mask);
            audio.aiDacrateChanged(ROM_PARAMS.systemtype);
        }
        return 0;
    }

    masked_write(&ai->regs[reg], value, mask);

    return 0;
}

void ai_end_of_dma_event(struct ai_controller* ai, unsigned int ai_event)
{
    if (ai->regs[AI_STATUS_REG] & 0x80000000) // full
    {
        ai->regs[AI_STATUS_REG] &= ~0x80000000;
        ai->fifo[0].delay = ai->fifo[1].delay;
        ai->fifo[0].length = ai->fifo[1].length;
        add_interupt_event_count(AI_INT, ai_event+ai->fifo[1].delay);
    }
    else
    {
        ai->regs[AI_STATUS_REG] &= ~0x40000000;
    }

    raise_rcp_interrupt(ai->r4300, MI_INTR_AI);
}
