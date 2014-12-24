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

#define read_word_in_memory() readmem[address>>16]()
#define read_byte_in_memory() readmemb[address>>16]()
#define read_hword_in_memory() readmemh[address>>16]()
#define read_dword_in_memory() readmemd[address>>16]()
#define write_word_in_memory() writemem[address>>16]()
#define write_byte_in_memory() writememb[address >>16]()
#define write_hword_in_memory() writememh[address >>16]()
#define write_dword_in_memory() writememd[address >>16]()

enum { SP_MEM_SIZE = 0x2000 };

extern uint32_t g_sp_mem[SP_MEM_SIZE/4];

extern unsigned int PIF_RAM[0x40/4];
extern unsigned char *PIF_RAMb;

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

extern unsigned int address, word;
extern unsigned char cpu_byte;
extern unsigned short hword;
extern unsigned long long dword, *rdword;

extern void (*readmem[0x10000])(void);
extern void (*readmemb[0x10000])(void);
extern void (*readmemh[0x10000])(void);
extern void (*readmemd[0x10000])(void);
extern void (*writemem[0x10000])(void);
extern void (*writememb[0x10000])(void);
extern void (*writememh[0x10000])(void);
extern void (*writememd[0x10000])(void);

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

void read_nothing(void);
void read_nothingh(void);
void read_nothingb(void);
void read_nothingd(void);
void read_nomem(void);
void read_nomemb(void);
void read_nomemh(void);
void read_nomemd(void);
void read_rdram(void);
void read_rdramb(void);
void read_rdramh(void);
void read_rdramd(void);
void read_rdramFB(void);
void read_rdramFBb(void);
void read_rdramFBh(void);
void read_rdramFBd(void);
void read_rdramreg(void);
void read_rdramregb(void);
void read_rdramregh(void);
void read_rdramregd(void);
void read_rsp_mem(void);
void read_rsp_memb(void);
void read_rsp_memh(void);
void read_rsp_memd(void);
void read_rsp_reg(void);
void read_rsp_regb(void);
void read_rsp_regh(void);
void read_rsp_regd(void);
void read_rsp(void);
void read_rspb(void);
void read_rsph(void);
void read_rspd(void);
void read_dp(void);
void read_dpb(void);
void read_dph(void);
void read_dpd(void);
void read_dps(void);
void read_dpsb(void);
void read_dpsh(void);
void read_dpsd(void);
void read_mi(void);
void read_mib(void);
void read_mih(void);
void read_mid(void);
void read_vi(void);
void read_vib(void);
void read_vih(void);
void read_vid(void);
void read_ai(void);
void read_aib(void);
void read_aih(void);
void read_aid(void);
void read_pi(void);
void read_pib(void);
void read_pih(void);
void read_pid(void);
void read_ri(void);
void read_rib(void);
void read_rih(void);
void read_rid(void);
void read_si(void);
void read_sib(void);
void read_sih(void);
void read_sid(void);
void read_flashram_status(void);
void read_flashram_statusb(void);
void read_flashram_statush(void);
void read_flashram_statusd(void);
void read_rom(void);
void read_romb(void);
void read_romh(void);
void read_romd(void);
void read_pif(void);
void read_pifb(void);
void read_pifh(void);
void read_pifd(void);

void write_nothing(void);
void write_nothingb(void);
void write_nothingh(void);
void write_nothingd(void);
void write_nomem(void);
void write_nomemb(void);
void write_nomemd(void);
void write_nomemh(void);
void write_rdram(void);
void write_rdramb(void);
void write_rdramh(void);
void write_rdramd(void);
void write_rdramFB(void);
void write_rdramFBb(void);
void write_rdramFBh(void);
void write_rdramFBd(void);
void write_rdramreg(void);
void write_rdramregb(void);
void write_rdramregh(void);
void write_rdramregd(void);
void write_rsp_mem(void);
void write_rsp_memb(void);
void write_rsp_memh(void);
void write_rsp_memd(void);
void write_rsp_reg(void);
void write_rsp_regb(void);
void write_rsp_regh(void);
void write_rsp_regd(void);
void write_rsp(void);
void write_rspb(void);
void write_rsph(void);
void write_rspd(void);
void write_dp(void);
void write_dpb(void);
void write_dph(void);
void write_dpd(void);
void write_dps(void);
void write_dpsb(void);
void write_dpsh(void);
void write_dpsd(void);
void write_mi(void);
void write_mib(void);
void write_mih(void);
void write_mid(void);
void write_vi(void);
void write_vib(void);
void write_vih(void);
void write_vid(void);
void write_ai(void);
void write_aib(void);
void write_aih(void);
void write_aid(void);
void write_pi(void);
void write_pib(void);
void write_pih(void);
void write_pid(void);
void write_ri(void);
void write_rib(void);
void write_rih(void);
void write_rid(void);
void write_si(void);
void write_sib(void);
void write_sih(void);
void write_sid(void);
void write_flashram_dummy(void);
void write_flashram_dummyb(void);
void write_flashram_dummyh(void);
void write_flashram_dummyd(void);
void write_flashram_command(void);
void write_flashram_commandb(void);
void write_flashram_commandh(void);
void write_flashram_commandd(void);
void write_rom(void);
void write_pif(void);
void write_pifb(void);
void write_pifh(void);
void write_pifd(void);

void update_MI_intr_mode_reg(void);
void update_MI_init_mask_reg(void);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
unsigned int *fast_mem_access(unsigned int address);

#endif

