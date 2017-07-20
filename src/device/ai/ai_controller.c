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

#include <string.h>

#include "backends/audio_out_backend.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/ri/ri_controller.h"
#include "device/vi/vi_controller.h"
#include "plugin/plugin.h"

enum
{
    AI_STATUS_BUSY = 0x40000000,
    AI_STATUS_FULL = 0x80000000
};


static uint32_t get_remaining_dma_length(struct ai_controller* ai)
{
    unsigned int next_ai_event;
    unsigned int remaining_dma_duration;
    const uint32_t* cp0_regs;

    if (ai->fifo[0].duration == 0)
        return 0;

    cp0_update_count(ai->r4300);
    next_ai_event = get_event(&ai->r4300->cp0.q, AI_INT);
    if (next_ai_event == 0)
        return 0;

    cp0_regs = r4300_cp0_regs(&ai->r4300->cp0);
    if (next_ai_event <= cp0_regs[CP0_COUNT_REG])
        return 0;

    remaining_dma_duration = next_ai_event - cp0_regs[CP0_COUNT_REG];

    uint64_t dma_length = (uint64_t)remaining_dma_duration * ai->fifo[0].length / ai->fifo[0].duration;
    return dma_length&~7;
}

static unsigned int get_dma_duration(struct ai_controller* ai)
{
    unsigned int samples_per_sec = ai->vi->clock / (1 + ai->regs[AI_DACRATE_REG]);
    unsigned int bytes_per_sample = 4; /* XXX: assume 16bit stereo - should depends on bitrate instead */
    unsigned int cpu_counts_per_sec = ai->vi->delay * ai->vi->expected_refresh_rate; /* estimate cpu counts/sec using VI */

    return ((uint64_t)ai->regs[AI_LEN_REG] * cpu_counts_per_sec) / (bytes_per_sample * samples_per_sec);
}


static void do_dma(struct ai_controller* ai, const struct ai_dma* dma)
{
    /* lazy initialization of sample format */
    if (ai->samples_format_changed)
    {
        unsigned int frequency = (ai->regs[AI_DACRATE_REG] == 0)
            ? 44100 /* default sample rate */
            : ai->vi->clock / (1 + ai->regs[AI_DACRATE_REG]);

        unsigned int bits = (ai->regs[AI_BITRATE_REG] == 0)
            ? 16 /* default bit rate */
            : 1 + ai->regs[AI_BITRATE_REG];

        audio_out_set_format(ai->aout, frequency, bits);

        ai->samples_format_changed = 0;
    }

    if (ai->stream_audio)
    {
        memcpy(ai->audio_buffer, &ai->ri->rdram.dram[dma->address/4], dma->length);
    }
    else
    {
        /* push audio samples to external sink */
        audio_out_push_samples(ai->aout, &ai->ri->rdram.dram[dma->address/4], dma->length);
    }

    /* schedule end of dma event */
    cp0_update_count(ai->r4300);
    add_interrupt_event(&ai->r4300->cp0, AI_INT, dma->duration);
}

static void fifo_push(struct ai_controller* ai)
{
    unsigned int duration = get_dma_duration(ai);

    if (ai->regs[AI_STATUS_REG] & AI_STATUS_BUSY)
    {
        ai->fifo[1].address = ai->regs[AI_DRAM_ADDR_REG];
        ai->fifo[1].length = ai->regs[AI_LEN_REG];
        ai->fifo[1].duration = duration;
        ai->regs[AI_STATUS_REG] |= AI_STATUS_FULL;
    }
    else
    {
        ai->fifo[0].address = ai->regs[AI_DRAM_ADDR_REG];
        ai->fifo[0].length = ai->regs[AI_LEN_REG];
        ai->fifo[0].duration = duration;
        ai->regs[AI_STATUS_REG] |= AI_STATUS_BUSY;

        do_dma(ai, &ai->fifo[0]);
    }
}

static void fifo_pop(struct ai_controller* ai)
{
    if (ai->regs[AI_STATUS_REG] & AI_STATUS_FULL)
    {
        ai->fifo[0].address = ai->fifo[1].address;
        ai->fifo[0].length = ai->fifo[1].length;
        ai->fifo[0].duration = ai->fifo[1].duration;
        ai->regs[AI_STATUS_REG] &= ~AI_STATUS_FULL;

        do_dma(ai, &ai->fifo[0]);
    }
    else
    {
        ai->regs[AI_STATUS_REG] &= ~AI_STATUS_BUSY;
    }
}


void init_ai(struct ai_controller* ai,
             struct r4300_core* r4300,
             struct ri_controller* ri,
             struct vi_controller* vi,
             struct audio_out_backend* aout, unsigned int fixed_audio_pos, unsigned int stream_audio)
{
    ai->r4300 = r4300;
    ai->ri = ri;
    ai->vi = vi;
    ai->aout = aout;
    ai->fixed_audio_pos = fixed_audio_pos;
    ai->stream_audio = stream_audio && audio.playAudio;
}

void poweron_ai(struct ai_controller* ai)
{
    memset(ai->regs, 0, AI_REGS_COUNT*sizeof(uint32_t));
    memset(ai->fifo, 0, AI_DMA_FIFO_SIZE*sizeof(struct ai_dma));
    memset(ai->audio_buffer, 0, 0x40000);
    ai->samples_format_changed = 0;
    ai->audio_pos = 0;
}

int read_ai_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct ai_controller* ai = (struct ai_controller*)opaque;
    uint32_t reg = ai_reg(address);

    if (reg == AI_LEN_REG)
    {
        *value = get_remaining_dma_length(ai);
        if (ai->stream_audio && *value != 0)
        {
            unsigned int address = ai->fifo[0].length - *value;
            unsigned char *p = (unsigned char*)&ai->ri->rdram.dram[ai->fifo[0].address/4];
            memcpy(&ai->audio_buffer[address], p + address, *value);
        }
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

    switch (reg)
    {
    case AI_DRAM_ADDR_REG:
        masked_write(&ai->regs[AI_DRAM_ADDR_REG], value, mask);
        if (ai->fixed_audio_pos) {
            if (ai->audio_pos == 0)
                ai->audio_pos = ai->regs[AI_DRAM_ADDR_REG];
            ai->regs[AI_DRAM_ADDR_REG] = ai->audio_pos;
        }
        return 0;

    case AI_LEN_REG:
        masked_write(&ai->regs[AI_LEN_REG], value, mask);
        fifo_push(ai);
        return 0;

    case AI_STATUS_REG:
        clear_rcp_interrupt(ai->r4300, MI_INTR_AI);
        return 0;

    case AI_BITRATE_REG:
    case AI_DACRATE_REG:
        /* lazy audio format setting */
        if ((ai->regs[reg]) != (value & mask))
            ai->samples_format_changed = 1;

        masked_write(&ai->regs[reg], value, mask);
        return 0;
    }

    masked_write(&ai->regs[reg], value, mask);

    return 0;
}

void ai_end_of_dma_event(void* opaque)
{
    struct ai_controller* ai = (struct ai_controller*)opaque;

    if (ai->stream_audio)
        audio.playAudio(ai->audio_buffer, ai->fifo[0].length);

    fifo_pop(ai);
    raise_rcp_interrupt(ai->r4300, MI_INTR_AI);
}

