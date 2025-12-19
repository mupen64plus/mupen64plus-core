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

enum
{
    /* SP_STATUS - write */
    SP_CLR_HALT       = 0x0000001,
    SP_SET_HALT       = 0x0000002,
    SP_CLR_BROKE      = 0x0000004,
    SP_CLR_INTR       = 0x0000008,
    SP_SET_INTR       = 0x0000010,
    SP_CLR_SSTEP      = 0x0000020,
    SP_SET_SSTEP      = 0x0000040,
    SP_CLR_INTR_BREAK = 0x0000080,
    SP_SET_INTR_BREAK = 0x0000100,
    SP_CLR_SIG0       = 0x0000200,
    SP_SET_SIG0       = 0x0000400,
    SP_CLR_SIG1       = 0x0000800,
    SP_SET_SIG1       = 0x0001000,
    SP_CLR_SIG2       = 0x0002000,
    SP_SET_SIG2       = 0x0004000,
    SP_CLR_SIG3       = 0x0008000,
    SP_SET_SIG3       = 0x0010000,
    SP_CLR_SIG4       = 0x0020000,
    SP_SET_SIG4       = 0x0040000,
    SP_CLR_SIG5       = 0x0080000,
    SP_SET_SIG5       = 0x0100000,
    SP_CLR_SIG6       = 0x0200000,
    SP_SET_SIG6       = 0x0400000,
    SP_CLR_SIG7       = 0x0800000,
    SP_SET_SIG7       = 0x1000000,
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

enum sp_dma_dir
{
    SP_DMA_READ,
    SP_DMA_WRITE
};

enum sp_rsp_wait
{
    WAIT_PENDING_SP_INT_BROKE = 0x1,
    WAIT_PENDING_SP_INT       = 0x2,
    WAIT_PENDING_DP_SYNC      = 0x4,
    WAIT_HALTED               = 0x8
};

enum { SP_DMA_FIFO_SIZE = 2} ;

struct sp_dma
{
    uint32_t dir;
    uint32_t length;
    uint32_t memaddr;
    uint32_t dramaddr;
};

struct rsp_core
{
    uint32_t* mem;
    uint32_t regs[SP_REGS_COUNT];
    uint32_t regs2[SP_REGS2_COUNT];
    uint32_t rsp_status;
    uint32_t first_run;
    uint32_t rsp_wait;

    struct mi_controller* mi;
    struct rdp_core* dp;
    struct ri_controller* ri;
    struct sp_dma fifo[SP_DMA_FIFO_SIZE];
};

static osal_inline uint32_t rsp_mem_address(uint32_t address)
{
    return (address & 0x1fff) >> 2;
}

static osal_inline uint32_t rsp_reg(uint32_t address)
{
    return (address & 0x1f) >> 2;
}

static osal_inline uint32_t rsp_reg2(uint32_t address)
{
    return (address & 0x1f) >> 2;
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
void rsp_end_of_dma_event(void* opaque);

void rsp_task_event(void* opaque);
void clear_rsp_wait(struct rsp_core* sp, uint32_t value);

#endif
