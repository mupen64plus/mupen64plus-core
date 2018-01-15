/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rsp_core.h                                              *
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

#ifndef M64P_DEVICE_RCP_RSP_RSP_CORE_H
#define M64P_DEVICE_RCP_RSP_RSP_CORE_H

#include <stdint.h>

#include "osal/preproc.h"

struct mi_controller;
struct rdp_core;
struct ri_controller;

enum { SP_MEM_SIZE = 0x2000 };

enum
{
    /* SP_STATUS - read */
    SP_STATUS_HALT       = 0x0001,
    SP_STATUS_BROKE      = 0x0002,
    SP_STATUS_DMA_BUSY   = 0x0004,
    SP_STATUS_DMA_FULL   = 0x0008,
    SP_STATUS_IO_FULL    = 0x0010,
    SP_STATUS_SSTEP      = 0x0020,
    SP_STATUS_INTR_BREAK = 0x0040,
    SP_STATUS_SIG0       = 0x0080,
    SP_STATUS_YIELD      = 0x0080,
    SP_STATUS_SIG1       = 0x0100,
    SP_STATUS_YIELDED    = 0x0100,
    SP_STATUS_SIG2       = 0x0200,
    SP_STATUS_TASKDONE   = 0x0200,
    SP_STATUS_SIG3       = 0x0400,
    SP_STATUS_SIG4       = 0x0800,
    SP_STATUS_SIG5       = 0x1000,
    SP_STATUS_SIG6       = 0x2000,
    SP_STATUS_SIG7       = 0x4000,
};

/* Synchronization event between RSP and CPU */
enum
{
    WAITING_SIG0_CLEARED = 0x00010000,
    WAITING_SIG1_CLEARED = 0x00020000,
    WAITING_SIG2_CLEARED = 0x00040000,
    WAITING_SIG3_CLEARED = 0x00080000,
    WAITING_SIG4_CLEARED = 0x00100000,
    WAITING_SIG5_CLEARED = 0x00200000,
    WAITING_SIG6_CLEARED = 0x00400000,
    WAITING_SIG7_CLEARED = 0x00800000,
    WAITING_SIG0_SET = 0x01000000,
    WAITING_SIG1_SET = 0x02000000,
    WAITING_SIG2_SET = 0x04000000,
    WAITING_SIG3_SET = 0x08000000,
    WAITING_SIG4_SET = 0x10000000,
    WAITING_SIG5_SET = 0x20000000,
    WAITING_SIG6_SET = 0x40000000,
    WAITING_SIG7_SET = 0x80000000,
};

enum sp_registers
{
    SP_MEM_ADDR_REG,
    SP_DRAM_ADDR_REG,
    SP_RD_LEN_REG,
    SP_WR_LEN_REG,
    SP_STATUS_REG,
    SP_DMA_FULL_REG,
    SP_DMA_BUSY_REG,
    SP_SEMAPHORE_REG,
    SP_REGS_COUNT
};

enum sp_registers2
{
    SP_PC_REG,
    SP_IBIST_REG,
    SP_REGS2_COUNT
};


struct rsp_core
{
    uint32_t* mem;
    uint32_t regs[SP_REGS_COUNT];
    uint32_t regs2[SP_REGS2_COUNT];
    uint32_t rsp_task_locked;

    struct mi_controller* mi;
    struct rdp_core* dp;
    struct ri_controller* ri;
};

static osal_inline uint32_t rsp_mem_address(uint32_t address)
{
    return (address & 0x1fff) >> 2;
}

static osal_inline uint32_t rsp_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static osal_inline uint32_t rsp_reg2(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

void init_rsp(struct rsp_core* sp,
              uint32_t* sp_mem,
              struct mi_controller* mi,
              struct rdp_core* dp,
              struct ri_controller* ri);

void poweron_rsp(struct rsp_core* sp);

void read_rsp_mem(void* opaque, uint32_t address, uint32_t* value);
void write_rsp_mem(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void read_rsp_regs(void* opaque, uint32_t address, uint32_t* value);
void write_rsp_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void read_rsp_regs2(void* opaque, uint32_t address, uint32_t* value);
void write_rsp_regs2(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void do_SP_Task(struct rsp_core* sp);

void rsp_interrupt_event(void* opaque);

#endif
