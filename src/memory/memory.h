/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "osal/preproc.h"

enum { SP_MEM_SIZE = 0x2000 };

extern uint32_t g_sp_mem[SP_MEM_SIZE/4];

enum { PIF_RAM_SIZE = 0x40 };

extern uint8_t g_pif_ram[PIF_RAM_SIZE];

enum { RDRAM_MAX_SIZE = 0x800000 };

extern ALIGN(16, uint32_t g_rdram[RDRAM_MAX_SIZE/4]);

enum rdram_registers
{
    RDRAM_CONFIG_REG,
    RDRAM_DEVICE_ID_REG,
    RDRAM_DELAY_REG,
    RDRAM_MODE_REG,
    RDRAM_REF_INTERVAL_REG,
    RDRAM_REF_ROW_REG,
    RDRAM_RAS_INTERVAL_REG,
    RDRAM_MIN_INTERVAL_REG,
    RDRAM_ADDR_SELECT_REG,
    RDRAM_DEVICE_MANUF_REG,
    RDRAM_REGS_COUNT
};

enum ri_registers
{
    RI_MODE_REG,
    RI_CONFIG_REG,
    RI_CURRENT_LOAD_REG,
    RI_SELECT_REG,
    RI_REFRESH_REG,
    RI_LATENCY_REG,
    RI_ERROR_REG,
    RI_WERROR_REG,
    RI_REGS_COUNT
};

extern uint32_t g_rdram_regs[RDRAM_REGS_COUNT];
extern uint32_t g_ri_regs[RI_REGS_COUNT];

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

extern uint32_t g_sp_regs[SP_REGS_COUNT];
extern uint32_t g_sp_regs2[SP_REGS2_COUNT];

enum dpc_registers
{
    DPC_START_REG,
    DPC_END_REG,
    DPC_CURRENT_REG,
    DPC_STATUS_REG,
    DPC_CLOCK_REG,
    DPC_BUFBUSY_REG,
    DPC_PIPEBUSY_REG,
    DPC_TMEM_REG,
    DPC_REGS_COUNT
};

enum dps_registers
{
    DPS_TBIST_REG,
    DPS_TEST_MODE_REG,
    DPS_BUFTEST_ADDR_REG,
    DPS_BUFTEST_DATA_REG,
    DPS_REGS_COUNT
};

extern uint32_t g_dpc_regs[DPC_REGS_COUNT];
extern uint32_t g_dps_regs[DPS_REGS_COUNT];

enum mi_registers
{
    MI_INIT_MODE_REG,
    MI_VERSION_REG,
    MI_INTR_REG,
    MI_INTR_MASK_REG,
    MI_REGS_COUNT
};

extern uint32_t g_mi_regs[MI_REGS_COUNT];

enum vi_registers
{
    VI_STATUS_REG,
    VI_ORIGIN_REG,
    VI_WIDTH_REG,
    VI_V_INTR_REG,
    VI_CURRENT_REG,
    VI_BURST_REG,
    VI_V_SYNC_REG,
    VI_H_SYNC_REG,
    VI_LEAP_REG,
    VI_H_START_REG,
    VI_V_START_REG,
    VI_V_BURST_REG,
    VI_X_SCALE_REG,
    VI_Y_SCALE_REG,
    VI_REGS_COUNT
};

extern uint32_t g_vi_regs[VI_REGS_COUNT];
extern unsigned int g_vi_delay;

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
    uint32_t length;
    unsigned int delay;
};

extern uint32_t g_ai_regs[AI_REGS_COUNT];
extern struct ai_dma g_ai_fifo[2];

enum pi_registers
{
    PI_DRAM_ADDR_REG,
    PI_CART_ADDR_REG,
    PI_RD_LEN_REG,
    PI_WR_LEN_REG,
    PI_STATUS_REG,
    PI_BSD_DOM1_LAT_REG,
    PI_BSD_DOM1_PWD_REG,
    PI_BSD_DOM1_PGS_REG,
    PI_BSD_DOM1_RLS_REG,
    PI_BSD_DOM2_LAT_REG,
    PI_BSD_DOM2_PWD_REG,
    PI_BSD_DOM2_PGS_REG,
    PI_BSD_DOM2_RLS_REG,
    PI_REGS_COUNT
};

extern uint32_t g_pi_regs[PI_REGS_COUNT];

enum si_registers
{
    SI_DRAM_ADDR_REG,
    SI_PIF_ADDR_RD64B_REG,
    SI_R2_REG, /* reserved */
    SI_R3_REG, /* reserved */
    SI_PIF_ADDR_WR64B_REG,
    SI_R5_REG, /* reserved */
    SI_STATUS_REG,
    SI_REGS_COUNT
};

extern uint32_t g_si_regs[SI_REGS_COUNT];

enum cic_type
{
    CIC_X101,
    CIC_X102,
    CIC_X103,
    CIC_X105,
    CIC_X106
};

extern enum cic_type g_cic_type;

#ifndef M64P_BIG_ENDIAN
#if defined(__GNUC__) && (__GNUC__ > 4  || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#define sl(x) __builtin_bswap32(x)
#else
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)
#endif
#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

int init_memory(void);

void map_region(uint16_t region,
                int type,
                int (*read32)(uint32_t,uint32_t*),
                int (*write32)(uint32_t,uint32_t,uint32_t));

int read_aligned_word(uint32_t address, uint32_t* value);
int write_aligned_word(uint32_t address, uint32_t value, uint32_t mask);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
unsigned int *fast_mem_access(unsigned int address);

#ifdef DBG
void activate_memory_break_read(uint32_t address);
void deactivate_memory_break_read(uint32_t address);
void activate_memory_break_write(uint32_t address);
void deactivate_memory_break_write(uint32_t address);
int get_memory_type(uint32_t address);
#endif

#endif

