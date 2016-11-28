/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - ai_controller.h                                         *
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

#ifndef M64P_AI_AI_CONTROLLER_H
#define M64P_AI_AI_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

struct r4300_core;
struct ri_controller;
struct vi_controller;
struct audio_out_backend;

enum ai_registers
{
    AI_DRAM_ADDR_REG,
    AI_LEN_REG,
    AI_CONTROL_REG,
    AI_STATUS_REG,
    AI_DACRATE_REG,
    AI_BITRATE_REG,
    AI_REGS_COUNT
};

struct ai_dma
{
    uint32_t address;
    uint32_t length;
    unsigned int duration;
};

enum { AI_DMA_FIFO_SIZE = 2 };

struct ai_controller
{
    uint32_t regs[AI_REGS_COUNT];
    struct ai_dma fifo[AI_DMA_FIFO_SIZE];
    unsigned int samples_format_changed;

    struct r4300_core* r4300;
    struct ri_controller* ri;
    struct vi_controller* vi;
    struct audio_out_backend* aout;
};

static uint32_t ai_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

void init_ai(struct ai_controller* ai,
             struct r4300_core* r4300,
             struct ri_controller* ri,
             struct vi_controller* vi,
             struct audio_out_backend* aout);

void poweron_ai(struct ai_controller* ai);

int read_ai_regs(void* opaque, uint32_t address, uint32_t* value);
int write_ai_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void ai_end_of_dma_event(struct ai_controller* ai);

#endif
