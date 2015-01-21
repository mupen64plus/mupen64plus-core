/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.c                                                *
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "api/m64p_types.h"

#include "memory.h"
#include "dma.h"
#include "pif.h"
#include "flashram.h"

#include "r4300/r4300.h"
#include "r4300/cached_interp.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/recomph.h"
#include "r4300/ops.h"
#include "r4300/tlb.h"

#include "api/callbacks.h"
#include "main/main.h"
#include "main/profile.h"
#include "main/rom.h"
#include "osal/preproc.h"
#include "plugin/plugin.h"
#include "r4300/new_dynarec/new_dynarec.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_breakpoints.h"
#endif

/* some functions prototypes */
static void read_nothing(void);
static void read_nothingb(void);
static void read_nothingh(void);
static void read_nothingd(void);
static void write_nothing(void);
static void write_nothingb(void);
static void write_nothingh(void);
static void write_nothingd(void);
static void read_nomem(void);
static void read_nomemb(void);
static void read_nomemh(void);
static void read_nomemd(void);
static void write_nomem(void);
static void write_nomemb(void);
static void write_nomemh(void);
static void write_nomemd(void);
static void read_rdramFB(void);
static void read_rdramFBb(void);
static void read_rdramFBh(void);
static void read_rdramFBd(void);
static void write_rdramFB(void);
static void write_rdramFBb(void);
static void write_rdramFBh(void);
static void write_rdramFBd(void);
static void read_rdramreg(void);
static void read_rdramregb(void);
static void read_rdramregh(void);
static void read_rdramregd(void);
static void write_rdramreg(void);
static void write_rdramregb(void);
static void write_rdramregh(void);
static void write_rdramregd(void);
static void read_rsp_mem(void);
static void read_rsp_memb(void);
static void read_rsp_memh(void);
static void read_rsp_memd(void);
static void write_rsp_mem(void);
static void write_rsp_memb(void);
static void write_rsp_memh(void);
static void write_rsp_memd(void);
static void read_rsp_reg(void);
static void read_rsp_regb(void);
static void read_rsp_regh(void);
static void read_rsp_regd(void);
static void write_rsp_reg(void);
static void write_rsp_regb(void);
static void write_rsp_regh(void);
static void write_rsp_regd(void);
static void read_rsp(void);
static void read_rspb(void);
static void read_rsph(void);
static void read_rspd(void);
static void write_rsp(void);
static void write_rspb(void);
static void write_rsph(void);
static void write_rspd(void);
static void read_dp(void);
static void read_dpb(void);
static void read_dph(void);
static void read_dpd(void);
static void write_dp(void);
static void write_dpb(void);
static void write_dph(void);
static void write_dpd(void);
static void read_dps(void);
static void read_dpsb(void);
static void read_dpsh(void);
static void read_dpsd(void);
static void write_dps(void);
static void write_dpsb(void);
static void write_dpsh(void);
static void write_dpsd(void);
static void read_mi(void);
static void read_mib(void);
static void read_mih(void);
static void read_mid(void);
static void write_mi(void);
static void write_mib(void);
static void write_mih(void);
static void write_mid(void);
static void read_vi(void);
static void read_vib(void);
static void read_vih(void);
static void read_vid(void);
static void write_vi(void);
static void write_vib(void);
static void write_vih(void);
static void write_vid(void);
static void read_ai(void);
static void read_aib(void);
static void read_aih(void);
static void read_aid(void);
static void write_ai(void);
static void write_aib(void);
static void write_aih(void);
static void write_aid(void);
static void read_pi(void);
static void read_pib(void);
static void read_pih(void);
static void read_pid(void);
static void write_pi(void);
static void write_pib(void);
static void write_pih(void);
static void write_pid(void);
static void read_ri(void);
static void read_rib(void);
static void read_rih(void);
static void read_rid(void);
static void write_ri(void);
static void write_rib(void);
static void write_rih(void);
static void write_rid(void);
static void read_si(void);
static void read_sib(void);
static void read_sih(void);
static void read_sid(void);
static void write_si(void);
static void write_sib(void);
static void write_sih(void);
static void write_sid(void);
static void read_flashram_status(void);
static void read_flashram_statusb(void);
static void read_flashram_statush(void);
static void read_flashram_statusd(void);
static void write_flashram_dummy(void);
static void write_flashram_dummyb(void);
static void write_flashram_dummyh(void);
static void write_flashram_dummyd(void);
static void write_flashram_command(void);
static void write_flashram_commandb(void);
static void write_flashram_commandh(void);
static void write_flashram_commandd(void);
static void read_rom(void);
static void read_romb(void);
static void read_romh(void);
static void read_romd(void);
static void write_rom(void);
static void read_pif(void);
static void read_pifb(void);
static void read_pifh(void);
static void read_pifd(void);
static void write_pif(void);
static void write_pifb(void);
static void write_pifh(void);
static void write_pifd(void);

/* definitions of the rcp's structures and memory area */
uint32_t g_rdram_regs[RDRAM_REGS_COUNT];
uint32_t g_ri_regs[RI_REGS_COUNT];
uint32_t g_mi_regs[MI_REGS_COUNT];
uint32_t g_pi_regs[PI_REGS_COUNT];
uint32_t g_sp_regs[SP_REGS_COUNT];
uint32_t g_sp_regs2[SP_REGS2_COUNT];
uint32_t g_si_regs[SI_REGS_COUNT];
uint32_t g_vi_regs[VI_REGS_COUNT];
unsigned int g_vi_delay;
uint32_t g_ai_regs[AI_REGS_COUNT];
struct ai_dma g_ai_fifo[2]; /* 0: current, 1:next */
uint32_t g_dpc_regs[DPC_REGS_COUNT];
uint32_t g_dps_regs[DPS_REGS_COUNT];

enum cic_type g_cic_type;

ALIGN(16, uint32_t g_rdram[RDRAM_MAX_SIZE/4]);

uint32_t g_sp_mem[SP_MEM_SIZE/4];
uint8_t g_pif_ram[PIF_RAM_SIZE];

#if NEW_DYNAREC != NEW_DYNAREC_ARM
// address : address of the read/write operation being done
unsigned int address = 0;
#endif

// values that are being written are stored in these variables
#if NEW_DYNAREC != NEW_DYNAREC_ARM
unsigned int word;
unsigned char cpu_byte;
unsigned short hword;
unsigned long long int dword;
#endif

// addresse where the read value will be stored
unsigned long long int* rdword;

// hash tables of read functions
void (*readmem[0x10000])(void);
void (*readmemb[0x10000])(void);
void (*readmemh[0x10000])(void);
void (*readmemd[0x10000])(void);

// hash tables of write functions
void (*writemem[0x10000])(void);
void (*writememb[0x10000])(void);
void (*writememd[0x10000])(void);
void (*writememh[0x10000])(void);

// the frameBufferInfos
static FrameBufferInfo frameBufferInfos[6];
static char framebufferRead[0x800];
static int firstFrameBufferSetting;

static enum cic_type detect_cic_type(const void* ipl3)
{
    size_t i;
    unsigned long long crc = 0;

    for (i = 0; i < 0xfc0/4; i++)
        crc += ((uint32_t*)ipl3)[i];

    switch(crc)
    {
        default:
            DebugMessage(M64MSG_WARNING, "Unknown CIC type (%08x)! using CIC 6102.", crc);
        case 0x000000D057C85244LL: return CIC_X102;
        case 0x000000D0027FDF31LL:
        case 0x000000CFFB631223LL: return CIC_X101;
        case 0x000000D6497E414BLL: return CIC_X103;
        case 0x0000011A49F60E96LL: return CIC_X105;
        case 0x000000D6D5BE5580LL: return CIC_X106;
    }

    /* never reached */
    return 0;
}

typedef int (*readfn)(void*,uint32_t,uint32_t*);
typedef int (*writefn)(void*,uint32_t,uint32_t,uint32_t);

static inline unsigned int bshift(uint32_t address)
{
    return ((address & 3) ^ 3) << 3;
}

static inline unsigned int hshift(uint32_t address)
{
    return ((address & 2) ^ 2) << 3;
}

static int readb(readfn read_word, void* opaque, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    unsigned shift = bshift(address);
    int result = read_word(opaque, address, &w);
    *value = (w >> shift) & 0xff;

    return result;
}

static int readh(readfn read_word, void* opaque, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    unsigned shift = hshift(address);
    int result = read_word(opaque, address, &w);
    *value = (w >> shift) & 0xffff;

    return result;
}

static int readw(readfn read_word, void* opaque, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    int result = read_word(opaque, address, &w);
    *value = w;

    return result;
}

static int readd(readfn read_word, void* opaque, uint32_t address, unsigned long long int* value)
{
    uint32_t w[2];
    int result =
    read_word(opaque, address    , &w[0]);
    read_word(opaque, address + 4, &w[1]);
    *value = ((uint64_t)w[0] << 32) | w[1];

    return result;
}

static int writeb(writefn write_word, void* opaque, uint32_t address, uint8_t value)
{
    unsigned int shift = bshift(address);
    uint32_t w = (uint32_t)value << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    return write_word(opaque, address, w, mask);
}

static int writeh(writefn write_word, void* opaque, uint32_t address, uint16_t value)
{
    unsigned int shift = hshift(address);
    uint32_t w = (uint32_t)value << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    return write_word(opaque, address, w, mask);
}

static int writew(writefn write_word, void* opaque, uint32_t address, uint32_t value)
{
    return write_word(opaque, address, value, ~0U);
}

static int writed(writefn write_word, void* opaque, uint32_t address, uint64_t value)
{
    int result =
    write_word(opaque, address    , value >> 32, ~0U);
    write_word(opaque, address + 4, value      , ~0U);

    return result;
}

#ifdef DBG
static int memtype[0x10000];
static void (*saved_readmemb[0x10000])(void);
static void (*saved_readmemh[0x10000])(void);
static void (*saved_readmem [0x10000])(void);
static void (*saved_readmemd[0x10000])(void);
static void (*saved_writememb[0x10000])(void);
static void (*saved_writememh[0x10000])(void);
static void (*saved_writemem [0x10000])(void);
static void (*saved_writememd[0x10000])(void);

static void readmemb_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 1,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    saved_readmemb[address>>16]();
}

static void readmemh_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 2,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    saved_readmemh[address>>16]();
}

static void readmem_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    saved_readmem[address>>16]();
}

static void readmemd_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 8,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    saved_readmemd[address>>16]();
}

static void writememb_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 1,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    return saved_writememb[address>>16]();
}

static void writememh_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 2,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    return saved_writememh[address>>16]();
}

static void writemem_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    return saved_writemem[address>>16]();
}

static void writememd_with_bp_checks(void)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 8,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    return saved_writememd[address>>16]();
}

void activate_memory_break_read(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_readmem[region] == NULL)
    {
        saved_readmemb[region] = readmemb[region];
        saved_readmemh[region] = readmemh[region];
        saved_readmem [region] = readmem [region];
        saved_readmemd[region] = readmemd[region];
        readmemb[region] = readmemb_with_bp_checks;
        readmemh[region] = readmemh_with_bp_checks;
        readmem [region] = readmem_with_bp_checks;
        readmemd[region] = readmemd_with_bp_checks;
    }
}

void deactivate_memory_break_read(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_readmem[region] != NULL)
    {
        readmemb[region] = saved_readmemb[region];
        readmemh[region] = saved_readmemh[region];
        readmem [region] = saved_readmem [region];
        readmemd[region] = saved_readmemd[region];
        saved_readmemb[region] = NULL;
        saved_readmemh[region] = NULL;
        saved_readmem [region] = NULL;
        saved_readmemd[region] = NULL;
    }
}

void activate_memory_break_write(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_writemem[region] == NULL)
    {
        saved_writememb[region] = writememb[region];
        saved_writememh[region] = writememh[region];
        saved_writemem [region] = writemem [region];
        saved_writememd[region] = writememd[region];
        writememb[region] = writememb_with_bp_checks;
        writememh[region] = writememh_with_bp_checks;
        writemem [region] = writemem_with_bp_checks;
        writememd[region] = writememd_with_bp_checks;
    }
}

void deactivate_memory_break_write(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_writemem[region] != NULL)
    {
        writememb[region] = saved_writememb[region];
        writememh[region] = saved_writememh[region];
        writemem [region] = saved_writemem [region];
        writememd[region] = saved_writememd[region];
        saved_writememb[region] = NULL;
        saved_writememh[region] = NULL;
        saved_writemem [region] = NULL;
        saved_writememd[region] = NULL;
    }
}

int get_memory_type(uint32_t address)
{
    return memtype[address >> 16];
}
#endif

#define R(x) read_ ## x ## b, read_ ## x ## h, read_ ## x, read_ ## x ## d
#define W(x) write_ ## x ## b, write_ ## x ## h, write_ ## x, write_ ## x ## d
#define RW(x) R(x), W(x)

int init_memory(void)
{
    int i;

#ifdef DBG
    memset(saved_readmem, 0, 0x10000*sizeof(saved_readmem[0]));
    memset(saved_writemem, 0, 0x10000*sizeof(saved_writemem[0]));
#endif

    /* clear mappings */
    for(i = 0; i < 0x10000; ++i)
    {
        map_region(i, M64P_MEM_NOMEM, RW(nomem));
    }

    /* init RDRAM */
    memset(g_rdram, 0, RDRAM_MAX_SIZE);

    /* map RDRAM */
    for(i = 0; i < /*0x40*/0x80; ++i)
    {
        map_region(0x8000+i, M64P_MEM_RDRAM, RW(rdram));
        map_region(0xa000+i, M64P_MEM_RDRAM, RW(rdram));
    }
    for(i = /*0x40*/0x80; i < 0x3f0; ++i)
    {
        map_region(0x8000+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa000+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init RDRAM registers */
    memset(g_rdram_regs, 0, RDRAM_REGS_COUNT*sizeof(g_rdram_regs[0]));

    /* map RDRAM registers */
    map_region(0x83f0, M64P_MEM_RDRAMREG, RW(rdramreg));
    map_region(0xa3f0, M64P_MEM_RDRAMREG, RW(rdramreg));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x83f0+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa3f0+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init RSP memory */
    memset(g_sp_mem, 0, SP_MEM_SIZE);

    /* map RSP memory */
    map_region(0x8400, M64P_MEM_RSPMEM, RW(rsp_mem));
    map_region(0xa400, M64P_MEM_RSPMEM, RW(rsp_mem));
    for(i = 1; i < 0x4; ++i)
    {
        map_region(0x8400+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa400+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init RSP registers */
    memset(g_sp_regs, 0, SP_REGS_COUNT*sizeof(g_sp_regs[0]));
    g_sp_regs[SP_STATUS_REG] = 1;

    /* map RSP registers (1) */
    map_region(0x8404, M64P_MEM_RSPREG, RW(rsp_reg));
    map_region(0xa404, M64P_MEM_RSPREG, RW(rsp_reg));
    for(i = 0x5; i < 0x8; ++i)
    {
        map_region(0x8400+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa400+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init RSP registers (2) */
    memset(g_sp_regs2, 0, SP_REGS2_COUNT*sizeof(g_sp_regs2[0]));

    /* map RSP registers (2) */
    map_region(0x8408, M64P_MEM_RSP, RW(rsp));
    map_region(0xa408, M64P_MEM_RSP, RW(rsp));
    for(i = 0x9; i < 0x10; ++i)
    {
        map_region(0x8400+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa400+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init DPC registers */
    memset(g_dpc_regs, 0, DPC_REGS_COUNT*sizeof(g_dpc_regs[0]));

    /* map DPC registers */
    map_region(0x8410, M64P_MEM_DP, RW(dp));
    map_region(0xa410, M64P_MEM_DP, RW(dp));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8410+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa410+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init DPS registers */
    memset(g_dps_regs, 0, DPS_REGS_COUNT*sizeof(g_dps_regs[0]));

    /* map DPS registers */
    map_region(0x8420, M64P_MEM_DPS, RW(dps));
    map_region(0xa420, M64P_MEM_DPS, RW(dps));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8420+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa420+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init MI registers */
    memset(g_mi_regs, 0, MI_REGS_COUNT*sizeof(g_mi_regs[0]));
    g_mi_regs[MI_VERSION_REG] = 0x02020102;

    /* map MI registers */
    map_region(0x8430, M64P_MEM_MI, RW(mi));
    map_region(0xa430, M64P_MEM_MI, RW(mi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8430+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa430+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init VI registers */
    memset(g_vi_regs, 0, VI_REGS_COUNT*sizeof(g_vi_regs[0]));

    /* map VI registers */
    map_region(0x8440, M64P_MEM_VI, RW(vi));
    map_region(0xa440, M64P_MEM_VI, RW(vi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8440+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa440+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init AI registers */
    memset(g_ai_regs, 0, AI_REGS_COUNT*sizeof(g_ai_regs[0]));
    memset(g_ai_fifo, 0, 2*sizeof(g_ai_fifo[0]));

    /* map AI registers */
    map_region(0x8450, M64P_MEM_AI, RW(ai));
    map_region(0xa450, M64P_MEM_AI, RW(ai));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8450+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa450+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init PI registers */
    memset(g_pi_regs, 0, PI_REGS_COUNT*sizeof(g_pi_regs[0]));

    /* map PI registers */
    map_region(0x8460, M64P_MEM_PI, RW(pi));
    map_region(0xa460, M64P_MEM_PI, RW(pi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8460+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa460+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init RI registers */
    memset(g_ri_regs, 0, RI_REGS_COUNT*sizeof(g_ri_regs[0]));

    /* map RI registers */
    map_region(0x8470, M64P_MEM_RI, RW(ri));
    map_region(0xa470, M64P_MEM_RI, RW(ri));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8470+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa470+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init SI registers */
    memset(g_si_regs, 0, SI_REGS_COUNT*sizeof(g_si_regs[0]));

    /* map SI registers */
    map_region(0x8480, M64P_MEM_SI, RW(si));
    map_region(0xa480, M64P_MEM_SI, RW(si));
    for(i = 0x481; i < 0x800; ++i)
    {
        map_region(0x8000+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa000+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* map flashram/sram */
    map_region(0x8800, M64P_MEM_FLASHRAMSTAT, R(flashram_status), W(flashram_dummy));
    map_region(0xa800, M64P_MEM_FLASHRAMSTAT, R(flashram_status), W(flashram_dummy));
    map_region(0x8801, M64P_MEM_NOTHING, R(nothing), W(flashram_command));
    map_region(0xa801, M64P_MEM_NOTHING, R(nothing), W(flashram_command));
    for(i = 0x802; i < 0x1000; ++i)
    {
        map_region(0x8000+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xa000+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* map cart ROM */
    for(i = 0; i < (rom_size >> 16); ++i)
    {
        map_region(0x9000+i, M64P_MEM_ROM, R(rom), W(nothing));
        map_region(0xb000+i, M64P_MEM_ROM, R(rom),
                   write_nothingb, write_nothingh, write_rom, write_nothingd);
    }
    for(i = (rom_size >> 16); i < 0xfc0; ++i)
    {
        map_region(0x9000+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xb000+i, M64P_MEM_NOTHING, RW(nothing));
    }

    /* init CIC type */
    g_cic_type = detect_cic_type(rom + 0x40);

    /* init PIF RAM */
    memset(g_pif_ram, 0, PIF_RAM_SIZE);

    /* map PIF RAM */
    map_region(0x9fc0, M64P_MEM_PIF, RW(pif));
    map_region(0xbfc0, M64P_MEM_PIF, RW(pif));
    for(i = 0xfc1; i < 0x1000; ++i)
    {
        map_region(0x9000+i, M64P_MEM_NOTHING, RW(nothing));
        map_region(0xb000+i, M64P_MEM_NOTHING, RW(nothing));
    }

    flashram_info.use_flashram = 0;
    init_flashram();

    frameBufferInfos[0].addr = 0;
    fast_memory = 1;
    firstFrameBufferSetting = 1;

    DebugMessage(M64MSG_VERBOSE, "Memory initialized");
    return 0;
}

static void map_region_t(uint16_t region, int type)
{
#ifdef DBG
    memtype[region] = type;
#else
    (void)region;
    (void)type;
#endif
}

static void map_region_r(uint16_t region,
        void (*read8)(void),
        void (*read16)(void),
        void (*read32)(void),
        void (*read64)(void))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
    {
        saved_readmemb[region] = read8;
        saved_readmemh[region] = read16;
        saved_readmem [region] = read32;
        saved_readmemd[region] = read64;
        readmemb[region] = readmemb_with_bp_checks;
        readmemh[region] = readmemh_with_bp_checks;
        readmem [region] = readmem_with_bp_checks;
        readmemd[region] = readmemd_with_bp_checks;
    }
    else
#endif
    {
        readmemb[region] = read8;
        readmemh[region] = read16;
        readmem [region] = read32;
        readmemd[region] = read64;
    }
}

static void map_region_w(uint16_t region,
        void (*write8)(void),
        void (*write16)(void),
        void (*write32)(void),
        void (*write64)(void))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
    {
        saved_writememb[region] = write8;
        saved_writememh[region] = write16;
        saved_writemem [region] = write32;
        saved_writememd[region] = write64;
        writememb[region] = writememb_with_bp_checks;
        writememh[region] = writememh_with_bp_checks;
        writemem [region] = writemem_with_bp_checks;
        writememd[region] = writememd_with_bp_checks;
    }
    else
#endif
    {
        writememb[region] = write8;
        writememh[region] = write16;
        writemem [region] = write32;
        writememd[region] = write64;
    }
}

void map_region(uint16_t region,
                int type,
                void (*read8)(void),
                void (*read16)(void),
                void (*read32)(void),
                void (*read64)(void),
                void (*write8)(void),
                void (*write16)(void),
                void (*write32)(void),
                void (*write64)(void))
{
    map_region_t(region, type);
    map_region_r(region, read8, read16, read32, read64);
    map_region_w(region, write8, write16, write32, write64);
}


static void update_MI_init_mode_reg(uint32_t w)
{
    g_mi_regs[MI_INIT_MODE_REG] &= ~0x7F; // init_length
    g_mi_regs[MI_INIT_MODE_REG] |= w & 0x7F;

    if (w & 0x80) // clear init_mode
        g_mi_regs[MI_INIT_MODE_REG] &= ~0x80;
    if (w & 0x100) // set init_mode
        g_mi_regs[MI_INIT_MODE_REG] |= 0x80;

    if (w & 0x200) // clear ebus_test_mode
        g_mi_regs[MI_INIT_MODE_REG] &= ~0x100;
    if (w & 0x400) // set ebus_test_mode
        g_mi_regs[MI_INIT_MODE_REG] |= 0x100;

    if (w & 0x800) // clear DP interupt
    {
        g_mi_regs[MI_INTR_REG] &= ~0x20;
        check_interupt();
    }

    if (w & 0x1000) // clear RDRAM_reg_mode
        g_mi_regs[MI_INIT_MODE_REG] &= ~0x200;
    if (w & 0x2000) // set RDRAM_reg_mode
        g_mi_regs[MI_INIT_MODE_REG] |= 0x200;
}

static void update_MI_intr_mask_reg(uint32_t w)
{
    if (w & 0x1)   g_mi_regs[MI_INTR_MASK_REG] &= ~0x1; // clear SP mask
    if (w & 0x2)   g_mi_regs[MI_INTR_MASK_REG] |= 0x1; // set SP mask
    if (w & 0x4)   g_mi_regs[MI_INTR_MASK_REG] &= ~0x2; // clear SI mask
    if (w & 0x8)   g_mi_regs[MI_INTR_MASK_REG] |= 0x2; // set SI mask
    if (w & 0x10)  g_mi_regs[MI_INTR_MASK_REG] &= ~0x4; // clear AI mask
    if (w & 0x20)  g_mi_regs[MI_INTR_MASK_REG] |= 0x4; // set AI mask
    if (w & 0x40)  g_mi_regs[MI_INTR_MASK_REG] &= ~0x8; // clear VI mask
    if (w & 0x80)  g_mi_regs[MI_INTR_MASK_REG] |= 0x8; // set VI mask
    if (w & 0x100) g_mi_regs[MI_INTR_MASK_REG] &= ~0x10; // clear PI mask
    if (w & 0x200) g_mi_regs[MI_INTR_MASK_REG] |= 0x10; // set PI mask
    if (w & 0x400) g_mi_regs[MI_INTR_MASK_REG] &= ~0x20; // clear DP mask
    if (w & 0x800) g_mi_regs[MI_INTR_MASK_REG] |= 0x20; // set DP mask
}

static void protect_framebuffers(void)
{
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite)
        gfx.fBGetFrameBufferInfo(frameBufferInfos);
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite
            && frameBufferInfos[0].addr)
    {
        int i;
        for (i=0; i<6; i++)
        {
            if (frameBufferInfos[i].addr)
            {
                int j;
                int start = frameBufferInfos[i].addr & 0x7FFFFF;
                int end = start + frameBufferInfos[i].width*
                          frameBufferInfos[i].height*
                          frameBufferInfos[i].size - 1;
                int start1 = start;
                int end1 = end;
                start >>= 16;
                end >>= 16;
                for (j=start; j<=end; j++)
                {
                    map_region(0x8000+j, M64P_MEM_RDRAM, RW(rdramFB));
                    map_region(0xa000+j, M64P_MEM_RDRAM, RW(rdramFB));
                }
                start <<= 4;
                end <<= 4;
                for (j=start; j<=end; j++)
                {
                    if (j>=start1 && j<=end1) framebufferRead[j]=1;
                    else framebufferRead[j] = 0;
                }

                if (firstFrameBufferSetting)
                {
                    firstFrameBufferSetting = 0;
                    fast_memory = 0;
                    for (j=0; j<0x100000; j++)
                        invalid_code[j] = 1;
                }
            }
        }
    }
}

static void unprotect_framebuffers(void)
{
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite &&
            frameBufferInfos[0].addr)
    {
        int i;
        for (i=0; i<6; i++)
        {
            if (frameBufferInfos[i].addr)
            {
                int j;
                int start = frameBufferInfos[i].addr & 0x7FFFFF;
                int end = start + frameBufferInfos[i].width*
                          frameBufferInfos[i].height*
                          frameBufferInfos[i].size - 1;
                start = start >> 16;
                end = end >> 16;

                for (j=start; j<=end; j++)
                {
                    map_region(0x8000+j, M64P_MEM_RDRAM, RW(rdram));
                    map_region(0xa000+j, M64P_MEM_RDRAM, RW(rdram));
                }
            }
        }
    }
}


static void do_SP_Task(void)
{
    int save_pc = g_sp_regs2[SP_PC_REG] & ~0xFFF;
    if (g_sp_mem[0xFC0/4] == 1)
    {
        if (g_dpc_regs[DPC_STATUS_REG] & 0x2) // DP frozen (DK64, BC)
        {
            // don't do the task now
            // the task will be done when DP is unfreezed (see update_DPC)
            return;
        }

        unprotect_framebuffers();

        //gfx.processDList();
        g_sp_regs2[SP_PC_REG] &= 0xFFF;
        timed_section_start(TIMED_SECTION_GFX);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_GFX);
        g_sp_regs2[SP_PC_REG] |= save_pc;
        new_frame();

        update_count();
        if (g_mi_regs[MI_INTR_REG] & 0x1)
            add_interupt_event(SP_INT, 1000);
        if (g_mi_regs[MI_INTR_REG] & 0x20)
            add_interupt_event(DP_INT, 1000);
        g_mi_regs[MI_INTR_REG] &= ~0x21;
        g_sp_regs[SP_STATUS_REG] &= ~0x303;

        protect_framebuffers();
    }
    else if (g_sp_mem[0xFC0/4] == 2)
    {
        //audio.processAList();
        g_sp_regs2[SP_PC_REG] &= 0xFFF;
        timed_section_start(TIMED_SECTION_AUDIO);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_AUDIO);
        g_sp_regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (g_mi_regs[MI_INTR_REG] & 0x1)
            add_interupt_event(SP_INT, 4000/*500*/);
        g_mi_regs[MI_INTR_REG] &= ~0x1;
        g_sp_regs[SP_STATUS_REG] &= ~0x303;
        
    }
    else
    {
        g_sp_regs2[SP_PC_REG] &= 0xFFF;
        rsp.doRspCycles(0xFFFFFFFF);
        g_sp_regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (g_mi_regs[MI_INTR_REG] & 0x1)
            add_interupt_event(SP_INT, 0/*100*/);
        g_mi_regs[MI_INTR_REG] &= ~0x1;
        g_sp_regs[SP_STATUS_REG] &= ~0x203;
    }
}

static void update_SP(uint32_t w)
{
    if (w & 0x1) // clear halt
        g_sp_regs[SP_STATUS_REG] &= ~0x1;
    if (w & 0x2) // set halt
        g_sp_regs[SP_STATUS_REG] |= 0x1;

    if (w & 0x4) // clear broke
        g_sp_regs[SP_STATUS_REG] &= ~0x2;

    if (w & 0x8) // clear SP interupt
    {
        g_mi_regs[MI_INTR_REG] &= ~1;
        check_interupt();
    }

    if (w & 0x10) // set SP interupt
    {
        g_mi_regs[MI_INTR_REG] |= 1;
        check_interupt();
    }

    if (w & 0x20) // clear single step
        g_sp_regs[SP_STATUS_REG] &= ~0x20;
    if (w & 0x40) // set single step
        g_sp_regs[SP_STATUS_REG] |= 0x20;

    if (w & 0x80) // clear interrupt on break
        g_sp_regs[SP_STATUS_REG] &= ~0x40;
    if (w & 0x100) // set interrupt on break
        g_sp_regs[SP_STATUS_REG] |= 0x40;

    if (w & 0x200) // clear signal 0
        g_sp_regs[SP_STATUS_REG] &= ~0x80;
    if (w & 0x400) // set signal 0
        g_sp_regs[SP_STATUS_REG] |= 0x80;

    if (w & 0x800) // clear signal 1
        g_sp_regs[SP_STATUS_REG] &= ~0x100;
    if (w & 0x1000) // set signal 1
        g_sp_regs[SP_STATUS_REG] |= 0x100;

    if (w & 0x2000) // clear signal 2
        g_sp_regs[SP_STATUS_REG] &= ~0x200;
    if (w & 0x4000) // set signal 2
        g_sp_regs[SP_STATUS_REG] |= 0x200;

    if (w & 0x8000) // clear signal 3
        g_sp_regs[SP_STATUS_REG] &= ~0x400;
    if (w & 0x10000) // set signal 3
        g_sp_regs[SP_STATUS_REG] |= 0x400;

    if (w & 0x20000) // clear signal 4
        g_sp_regs[SP_STATUS_REG] &= ~0x800;
    if (w & 0x40000) // set signal 4
        g_sp_regs[SP_STATUS_REG] |= 0x800;

    if (w & 0x80000) // clear signal 5
        g_sp_regs[SP_STATUS_REG] &= ~0x1000;
    if (w & 0x100000) // set signal 5
        g_sp_regs[SP_STATUS_REG] |= 0x1000;

    if (w & 0x200000) // clear signal 6
        g_sp_regs[SP_STATUS_REG] &= ~0x2000;
    if (w & 0x400000) // set signal 6
        g_sp_regs[SP_STATUS_REG] |= 0x2000;

    if (w & 0x800000) // clear signal 7
        g_sp_regs[SP_STATUS_REG] &= ~0x4000;
    if (w & 0x1000000) // set signal 7
        g_sp_regs[SP_STATUS_REG] |= 0x4000;

    //if (get_event(SP_INT)) return;
    if (!(w & 0x1) &&
            !(w & 0x4)) return;
    if (!(g_sp_regs[SP_STATUS_REG] & 0x3)) // !halt && !broke
        do_SP_Task();
}

static void update_DPC(uint32_t w)
{
    if (w & 0x1) // clear xbus_dmem_dma
        g_dpc_regs[DPC_STATUS_REG] &= ~0x1;
    if (w & 0x2) // set xbus_dmem_dma
        g_dpc_regs[DPC_STATUS_REG] |= 0x1;

    if (w & 0x4) // clear freeze
    {
        g_dpc_regs[DPC_STATUS_REG] &= ~0x2;

        // see do_SP_task for more info
        if (!(g_sp_regs[SP_STATUS_REG] & 0x3)) // !halt && !broke
            do_SP_Task();
    }
    if (w & 0x8) // set freeze
        g_dpc_regs[DPC_STATUS_REG] |= 0x2;

    if (w & 0x10) // clear flush
        g_dpc_regs[DPC_STATUS_REG] &= ~0x4;
    if (w & 0x20) // set flush
        g_dpc_regs[DPC_STATUS_REG] |= 0x4;
}

static void invalidate_code(uint32_t address)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops !=
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;
}

static void pre_framebuffer_read(uint32_t address)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    framebufferRead[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                framebufferRead[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
}

static void pre_framebuffer_write(uint32_t address, size_t size)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address, size);
        }
    }
}


static void read_nothing(void)
{
    if (address == 0xa5000508) *rdword = 0xFFFFFFFF;
    else *rdword = 0;
}

static void read_nothingb(void)
{
    *rdword = 0;
}

static void read_nothingh(void)
{
    *rdword = 0;
}

static void read_nothingd(void)
{
    *rdword = 0;
}

static void write_nothing(void)
{
}

static void write_nothingb(void)
{
}

static void write_nothingh(void)
{
}

static void write_nothingd(void)
{
}

static void read_nomem(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_word_in_memory();
}

static void read_nomemb(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_byte_in_memory();
}

static void read_nomemh(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_hword_in_memory();
}

static void read_nomemd(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_dword_in_memory();
}

static void write_nomem(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_word_in_memory();
}

static void write_nomemb(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_byte_in_memory();
}

static void write_nomemh(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_hword_in_memory();
}

static void write_nomemd(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_dword_in_memory();
}

static inline uint32_t rdram_ram_address(uint32_t address)
{
    return (address & 0xffffff) >> 2;
}

static int read_rdram_ram(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t addr = rdram_ram_address(address);

    *value = g_rdram[addr];

    return 0;
}

static int write_rdram_ram(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = rdram_ram_address(address);

    masked_write(&g_rdram[addr], value, mask);

    return 0;
}

void read_rdram(void)
{
    readw(read_rdram_ram, NULL, address, rdword);
}

void read_rdramb(void)
{
    readb(read_rdram_ram, NULL, address, rdword);
}

void read_rdramh(void)
{
    readh(read_rdram_ram, NULL, address, rdword);
}

void read_rdramd(void)
{
    readd(read_rdram_ram, NULL, address, rdword);
}

static void read_rdramFB(void)
{
    pre_framebuffer_read(address);
    read_rdram();
}

static void read_rdramFBb(void)
{
    pre_framebuffer_read(address);
    read_rdramb();
}

static void read_rdramFBh(void)
{
    pre_framebuffer_read(address);
    read_rdramh();
}

static void read_rdramFBd(void)
{
    pre_framebuffer_read(address);
    read_rdramd();
}

void write_rdram(void)
{
    writew(write_rdram_ram, NULL, address, word);
}

void write_rdramb(void)
{
    writeb(write_rdram_ram, NULL, address, cpu_byte);
}

void write_rdramh(void)
{
    writeh(write_rdram_ram, NULL, address, hword);
}

void write_rdramd(void)
{
    writed(write_rdram_ram, NULL, address, dword);
}

static void write_rdramFB(void)
{
    pre_framebuffer_write(address, 4);
    write_rdram();
}

static void write_rdramFBb(void)
{
    pre_framebuffer_write(address, 1);
    write_rdramb();
}

static void write_rdramFBh(void)
{
    pre_framebuffer_write(address, 2);
    write_rdramh();
}

static void write_rdramFBd(void)
{
    pre_framebuffer_write(address, 8);
    write_rdramd();
}

static inline uint32_t rdram_reg(uint32_t address)
{
    return (address & 0x3ff) >> 2;
}

static int read_rdram_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = rdram_reg(address);

    *value = g_rdram_regs[reg];

    return 0;
}

static int write_rdram_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rdram_reg(address);

    masked_write(&g_rdram_regs[reg], value, mask);

    return 0;
}

static void read_rdramreg(void)
{
    readw(read_rdram_regs, NULL, address, rdword);
}

static void read_rdramregb(void)
{
    readb(read_rdram_regs, NULL, address, rdword);
}

static void read_rdramregh(void)
{
    readh(read_rdram_regs, NULL, address, rdword);
}

static void read_rdramregd(void)
{
    readd(read_rdram_regs, NULL, address, rdword);
}

static void write_rdramreg(void)
{
    writew(write_rdram_regs, NULL, address, word);
}

static void write_rdramregb(void)
{
    writeb(write_rdram_regs, NULL, address, cpu_byte);
}

static void write_rdramregh(void)
{
    writeh(write_rdram_regs, NULL, address, hword);
}

static void write_rdramregd(void)
{
    writed(write_rdram_regs, NULL, address, dword);
}

static inline uint32_t rsp_mem_address(uint32_t address)
{
    return (address & 0x1fff) >> 2;
}

static int read_rspmem(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t addr = rsp_mem_address(address);

    *value = g_sp_mem[addr];

    return 0;
}

static int write_rspmem(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = rsp_mem_address(address);

    masked_write(&g_sp_mem[addr], value, mask);

    return 0;
}

static void read_rsp_mem(void)
{
    readw(read_rspmem, NULL, address, rdword);
}

static void read_rsp_memb(void)
{
    readb(read_rspmem, NULL, address, rdword);
}

static void read_rsp_memh(void)
{
    readh(read_rspmem, NULL, address, rdword);
}

static void read_rsp_memd(void)
{
    readd(read_rspmem, NULL, address, rdword);
}

static void write_rsp_mem(void)
{
    writew(write_rspmem, NULL, address, word);
}

static void write_rsp_memb(void)
{
    writeb(write_rspmem, NULL, address, cpu_byte);
}

static void write_rsp_memh(void)
{
    writeh(write_rspmem, NULL, address, hword);
}

static void write_rsp_memd(void)
{
    writed(write_rspmem, NULL, address, dword);
}


static inline uint32_t rsp_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_rsp_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg(address);

    *value = g_sp_regs[reg];

    if (reg == SP_SEMAPHORE_REG)
    {
        g_sp_regs[SP_SEMAPHORE_REG] = 1;
    }

    return 0;
}

static int write_rsp_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rsp_reg(address);

    switch(reg)
    {
    case SP_STATUS_REG:
        update_SP(value & mask);
    case SP_DMA_FULL_REG:
    case SP_DMA_BUSY_REG:
        return 0;
    }

    masked_write(&g_sp_regs[reg], value, mask);

    switch(reg)
    {
    case SP_RD_LEN_REG:
        dma_sp_write();
        break;
    case SP_WR_LEN_REG:
        dma_sp_read();
        break;
    case SP_SEMAPHORE_REG:
        g_sp_regs[SP_SEMAPHORE_REG] = 0;
        break;
    }

    return 0;
}

static void read_rsp_reg(void)
{
    readw(read_rsp_regs, NULL, address, rdword);
}

static void read_rsp_regb(void)
{
    readb(read_rsp_regs, NULL, address, rdword);
}

static void read_rsp_regh(void)
{
    readh(read_rsp_regs, NULL, address, rdword);
}

static void read_rsp_regd(void)
{
    readd(read_rsp_regs, NULL, address, rdword);
}

static void write_rsp_reg(void)
{
    writew(write_rsp_regs, NULL, address, word);
}

static void write_rsp_regb(void)
{
    writeb(write_rsp_regs, NULL, address, cpu_byte);
}

static void write_rsp_regh(void)
{
    writeh(write_rsp_regs, NULL, address, hword);
}

static void write_rsp_regd(void)
{
    writed(write_rsp_regs, NULL, address, dword);
}


static inline uint32_t rsp_reg2(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_rsp_regs2(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg2(address);

    *value = g_sp_regs2[reg];

    return 0;
}

static int write_rsp_regs2(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rsp_reg2(address);

    masked_write(&g_sp_regs2[reg], value, mask);

    return 0;
}

static void read_rsp(void)
{
    readw(read_rsp_regs2, NULL, address, rdword);
}

static void read_rspb(void)
{
    readb(read_rsp_regs2, NULL, address, rdword);
}

static void read_rsph(void)
{
    readh(read_rsp_regs2, NULL, address, rdword);
}

static void read_rspd(void)
{
    readd(read_rsp_regs2, NULL, address, rdword);
}

static void write_rsp(void)
{
    writew(write_rsp_regs2, NULL, address, word);
}

static void write_rspb(void)
{
    writeb(write_rsp_regs2, NULL, address, cpu_byte);
}

static void write_rsph(void)
{
    writeh(write_rsp_regs2, NULL, address, hword);
}

static void write_rspd(void)
{
    writed(write_rsp_regs2, NULL, address, dword);
}


static inline uint32_t dpc_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_dpc_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = dpc_reg(address);

    *value = g_dpc_regs[reg];

    return 0;
}

static int write_dpc_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = dpc_reg(address);

    switch(reg)
    {
    case DPC_STATUS_REG:
        update_DPC(value & mask);
    case DPC_CURRENT_REG:
    case DPC_CLOCK_REG:
    case DPC_BUFBUSY_REG:
    case DPC_PIPEBUSY_REG:
    case DPC_TMEM_REG:
        return 0;
    }

    masked_write(&g_dpc_regs[reg], value, mask);

    switch(reg)
    {
    case DPC_START_REG:
        g_dpc_regs[DPC_CURRENT_REG] = g_dpc_regs[DPC_START_REG];
        break;
    case DPC_END_REG:
        gfx.processRDPList();
        g_mi_regs[MI_INTR_REG] |= 0x20;
        check_interupt();
        break;
    }

    return 0;
}

static void read_dp(void)
{
    readw(read_dpc_regs, NULL, address, rdword);
}

static void read_dpb(void)
{
    readb(read_dpc_regs, NULL, address, rdword);
}

static void read_dph(void)
{
    readh(read_dpc_regs, NULL, address, rdword);
}

static void read_dpd(void)
{
    readd(read_dpc_regs, NULL, address, rdword);
}

static void write_dp(void)
{
    writew(write_dpc_regs, NULL, address, word);
}

static void write_dpb(void)
{
    writeb(write_dpc_regs, NULL, address, cpu_byte);
}

static void write_dph(void)
{
    writeh(write_dpc_regs, NULL, address, hword);
}

static void write_dpd(void)
{
    writed(write_dpc_regs, NULL, address, dword);
}

static inline uint32_t dps_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_dps_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = dps_reg(address);

    *value = g_dps_regs[reg];

    return 0;
}

static int write_dps_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = dps_reg(address);

    masked_write(&g_dps_regs[reg], value, mask);

    return 0;
}

static void read_dps(void)
{
    readw(read_dps_regs, NULL, address, rdword);
}

static void read_dpsb(void)
{
    readb(read_dps_regs, NULL, address, rdword);
}

static void read_dpsh(void)
{
    readh(read_dps_regs, NULL, address, rdword);
}

static void read_dpsd(void)
{
    readd(read_dps_regs, NULL, address, rdword);
}

static void write_dps(void)
{
    writew(write_dps_regs, NULL, address, word);
}

static void write_dpsb(void)
{
    writeb(write_dps_regs, NULL, address, cpu_byte);
}

static void write_dpsh(void)
{
    writeh(write_dps_regs, NULL, address, hword);
}

static void write_dpsd(void)
{
    writed(write_dps_regs, NULL, address, dword);
}


static inline uint32_t mi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_mi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = mi_reg(address);

    *value = g_mi_regs[reg];

    return 0;
}

static int write_mi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = mi_reg(address);

    switch(reg)
    {
    case MI_INIT_MODE_REG:
        update_MI_init_mode_reg(value & mask);
        break;
    case MI_INTR_MASK_REG:
        update_MI_intr_mask_reg(value & mask);

        check_interupt();
        update_count();
        if (next_interupt <= g_cp0_regs[CP0_COUNT_REG]) gen_interupt();
        break;
    }

    return 0;
}

static void read_mi(void)
{
    readw(read_mi_regs, NULL, address, rdword);
}

static void read_mib(void)
{
    readb(read_mi_regs, NULL, address, rdword);
}

static void read_mih(void)
{
    readh(read_mi_regs, NULL, address, rdword);
}

static void read_mid(void)
{
    readd(read_mi_regs, NULL, address, rdword);
}

static void write_mi(void)
{
    writew(write_mi_regs, NULL, address, word);
}

static void write_mib(void)
{
    writeb(write_mi_regs, NULL, address, cpu_byte);
}

static void write_mih(void)
{
    writeh(write_mi_regs, NULL, address, hword);
}

static void write_mid(void)
{
    writed(write_mi_regs, NULL, address, dword);
}


static inline uint32_t vi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_vi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = vi_reg(address);

    if (reg == VI_CURRENT_REG)
    {
        update_count();
        g_vi_regs[VI_CURRENT_REG] = (g_vi_delay - (next_vi - g_cp0_regs[CP0_COUNT_REG]))/1500;
        g_vi_regs[VI_CURRENT_REG] = (g_vi_regs[VI_CURRENT_REG] & (~1)) | vi_field;
    }

    *value = g_vi_regs[reg];

    return 0;
}

static int write_vi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = vi_reg(address);

    switch(reg)
    {
    case VI_STATUS_REG:
        if ((g_vi_regs[VI_STATUS_REG] & mask) != (value & mask))
        {
            masked_write(&g_vi_regs[VI_STATUS_REG], value, mask);
            gfx.viStatusChanged();
        }
        return 0;

    case VI_WIDTH_REG:
        if ((g_vi_regs[VI_WIDTH_REG] & mask) != (value & mask))
        {
            masked_write(&g_vi_regs[VI_WIDTH_REG], value, mask);
            gfx.viWidthChanged();
        }
        return 0;

    case VI_CURRENT_REG:
        g_mi_regs[MI_INTR_REG] &= ~0x8;
        check_interupt();
        return 0;
    }

    masked_write(&g_vi_regs[reg], value, mask);

    return 0;
}

static void read_vi(void)
{
    readw(read_vi_regs, NULL, address, rdword);
}

static void read_vib(void)
{
    readb(read_vi_regs, NULL, address, rdword);
}

static void read_vih(void)
{
    readh(read_vi_regs, NULL, address, rdword);
}

static void read_vid(void)
{
    readd(read_vi_regs, NULL, address, rdword);
}

static void write_vi(void)
{
    writew(write_vi_regs, NULL, address, word);
}

static void write_vib(void)
{
    writeb(write_vi_regs, NULL, address, cpu_byte);
}

static void write_vih(void)
{
    writeh(write_vi_regs, NULL, address, hword);
}

static void write_vid(void)
{
    writed(write_vi_regs, NULL, address, dword);
}


static inline uint32_t ai_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_ai_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = ai_reg(address);

    if (reg == AI_LEN_REG)
    {
        update_count();
        if (g_ai_fifo[0].delay != 0 && get_event(AI_INT) != 0 && (get_event(AI_INT)-g_cp0_regs[CP0_COUNT_REG]) < 0x80000000)
            *value = ((get_event(AI_INT)-g_cp0_regs[CP0_COUNT_REG])*(long long)g_ai_fifo[0].length)/
                      g_ai_fifo[0].delay;
        else
            *value = 0;
    }
    else
    {
        *value = g_ai_regs[reg];
    }

    return 0;
}

static int write_ai_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = ai_reg(address);

    unsigned int freq,delay=0;
    switch (reg)
    {
    case AI_LEN_REG:
        masked_write(&g_ai_regs[AI_LEN_REG], value, mask);
        audio.aiLenChanged();

        freq = ROM_PARAMS.aidacrate / (g_ai_regs[AI_DACRATE_REG]+1);
        if (freq)
            delay = (unsigned int) (((unsigned long long)g_ai_regs[AI_LEN_REG]*g_vi_delay*ROM_PARAMS.vilimit)/(freq*4));

        if (g_ai_regs[AI_STATUS_REG] & 0x40000000) // busy
        {
            g_ai_fifo[1].delay = delay;
            g_ai_fifo[1].length = g_ai_regs[AI_LEN_REG];
            g_ai_regs[AI_STATUS_REG] |= 0x80000000;
        }
        else
        {
            g_ai_fifo[0].delay = delay;
            g_ai_fifo[0].length = g_ai_regs[AI_LEN_REG];
            update_count();
            add_interupt_event(AI_INT, delay);
            g_ai_regs[AI_STATUS_REG] |= 0x40000000;
        }
        return 0;

    case AI_STATUS_REG:
        g_mi_regs[MI_INTR_REG] &= ~0x4;
        check_interupt();
        return 0;

    case AI_DACRATE_REG:
        if ((g_ai_regs[AI_DACRATE_REG] & mask) != (value & mask))
        {
            masked_write(&g_ai_regs[AI_DACRATE_REG], value, mask);
            audio.aiDacrateChanged(ROM_PARAMS.systemtype);
        }
        return 0;
    }

    masked_write(&g_ai_regs[reg], value, mask);

    return 0;
}

static void read_ai(void)
{
    readw(read_ai_regs, NULL, address, rdword);
}

static void read_aib(void)
{
    readb(read_ai_regs, NULL, address, rdword);
}

static void read_aih(void)
{
    readh(read_ai_regs, NULL, address, rdword);
}

static void read_aid(void)
{
    readd(read_ai_regs, NULL, address, rdword);
}

static void write_ai(void)
{
    writew(write_ai_regs, NULL, address, word);
}

static void write_aib(void)
{
    writeb(write_ai_regs, NULL, address, cpu_byte);
}

static void write_aih(void)
{
    writeh(write_ai_regs, NULL, address, hword);
}

static void write_aid(void)
{
    writed(write_ai_regs, NULL, address, dword);
}


static inline uint32_t pi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_pi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = pi_reg(address);

    *value = g_pi_regs[reg];

    return 0;
}

static int write_pi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = pi_reg(address);

    switch (reg)
    {
    case PI_RD_LEN_REG:
        masked_write(&g_pi_regs[PI_RD_LEN_REG], value, mask);
        dma_pi_read();
        return 0;

    case PI_WR_LEN_REG:
        masked_write(&g_pi_regs[PI_WR_LEN_REG], value, mask);
        dma_pi_write();
        return 0;

    case PI_STATUS_REG:
        if (value & mask & 2) g_mi_regs[MI_INTR_REG] &= ~0x10;
        check_interupt();
        return 0;

    case PI_BSD_DOM1_LAT_REG:
    case PI_BSD_DOM1_PWD_REG:
    case PI_BSD_DOM1_PGS_REG:
    case PI_BSD_DOM1_RLS_REG:
    case PI_BSD_DOM2_LAT_REG:
    case PI_BSD_DOM2_PWD_REG:
    case PI_BSD_DOM2_PGS_REG:
    case PI_BSD_DOM2_RLS_REG:
        masked_write(&g_pi_regs[reg], value & 0xff, mask);
        return 0;
    }

    masked_write(&g_pi_regs[reg], value, mask);

    return 0;
}


static void read_pi(void)
{
    readw(read_pi_regs, NULL, address, rdword);
}

static void read_pib(void)
{
    readb(read_pi_regs, NULL, address, rdword);
}

static void read_pih(void)
{
    readh(read_pi_regs, NULL, address, rdword);
}

static void read_pid(void)
{
    readd(read_pi_regs, NULL, address, rdword);
}

static void write_pi(void)
{
    writew(write_pi_regs, NULL, address, word);
}

static void write_pib(void)
{
    writeb(write_pi_regs, NULL, address, cpu_byte);
}

static void write_pih(void)
{
    writeh(write_pi_regs, NULL, address, hword);
}

static void write_pid(void)
{
    writed(write_pi_regs, NULL, address, dword);
}


static inline uint32_t ri_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_ri_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = ri_reg(address);

    *value = g_ri_regs[reg];

    return 0;
}

static int write_ri_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = ri_reg(address);

    masked_write(&g_ri_regs[reg], value, mask);

    return 0;
}

static void read_ri(void)
{
    readw(read_ri_regs, NULL, address, rdword);
}

static void read_rib(void)
{
    readb(read_ri_regs, NULL, address, rdword);
}

static void read_rih(void)
{
    readh(read_ri_regs, NULL, address, rdword);
}

static void read_rid(void)
{
    readd(read_ri_regs, NULL, address, rdword);
}

static void write_ri(void)
{
    writew(write_ri_regs, NULL, address, word);
}

static void write_rib(void)
{
    writeb(write_ri_regs, NULL, address, cpu_byte);
}

static void write_rih(void)
{
    writeh(write_ri_regs, NULL, address, hword);
}

static void write_rid(void)
{
    writed(write_ri_regs, NULL, address, dword);
}


static inline uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_si_regs(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t reg = si_reg(address);

    *value = g_si_regs[reg];

    return 0;
}

static int write_si_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = si_reg(address);

    switch (reg)
    {
    case SI_DRAM_ADDR_REG:
        masked_write(&g_si_regs[SI_DRAM_ADDR_REG], value, mask);
        break;

    case SI_PIF_ADDR_RD64B_REG:
        masked_write(&g_si_regs[SI_PIF_ADDR_RD64B_REG], value, mask);
        dma_si_read();
        break;

    case SI_PIF_ADDR_WR64B_REG:
        masked_write(&g_si_regs[SI_PIF_ADDR_WR64B_REG], value, mask);
        dma_si_write();
        break;

    case SI_STATUS_REG:
        g_mi_regs[MI_INTR_REG] &= ~0x2;
        g_si_regs[SI_STATUS_REG] &= ~0x1000;
        check_interupt();
        break;
    }

    return 0;
}

static void read_si(void)
{
    readw(read_si_regs, NULL, address, rdword);
}

static void read_sib(void)
{
    readb(read_si_regs, NULL, address, rdword);
}

static void read_sih(void)
{
    readh(read_si_regs, NULL, address, rdword);
}

static void read_sid(void)
{
    readd(read_si_regs, NULL, address, rdword);
}

static void write_si(void)
{
    writew(write_si_regs, NULL, address, word);
}

static void write_sib(void)
{
    writeb(write_si_regs, NULL, address, cpu_byte);
}

static void write_sih(void)
{
    writeh(write_si_regs, NULL, address, hword);
}

static void write_sid(void)
{
    writed(write_si_regs, NULL, address, dword);
}

static void read_flashram_status(void)
{
    if (flashram_info.use_flashram != -1 && ((address & 0xffff) == 0))
    {
        *rdword = flashram_status();
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown read in read_flashram_status()");
}

static void read_flashram_statusb(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusb() not implemented");
}

static void read_flashram_statush(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statush() not implemented");
}

static void read_flashram_statusd(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusd() not implemented");
}

static void write_flashram_dummy(void)
{
}

static void write_flashram_dummyb(void)
{
}

static void write_flashram_dummyh(void)
{
}

static void write_flashram_dummyd(void)
{
}

static void write_flashram_command(void)
{
    if (flashram_info.use_flashram != -1 && ((address & 0xffff) == 0))
    {
        flashram_command(word);
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown write in write_flashram_command()");
}

static void write_flashram_commandb(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandb() not implemented");
}

static void write_flashram_commandh(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandh() not implemented");
}

static void write_flashram_commandd(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandd() not implemented");
}

static unsigned int lastwrite = 0;

static inline uint32_t rom_address(uint32_t address)
{
    return (address & 0x03fffffc);
}

static int read_cart_rom(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t addr = rom_address(address);

    if (lastwrite)
    {
        *value = lastwrite;
        lastwrite = 0;
    }
    else
    {
        *value = *(uint32_t*)(rom + addr);
    }

    return 0;
}

static int write_cart_rom(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    lastwrite = value & mask;

    return 0;
}


static void read_rom(void)
{
    readw(read_cart_rom, NULL, address, rdword);
}

static void read_romb(void)
{
    readb(read_cart_rom, NULL, address, rdword);
}

static void read_romh(void)
{
    readh(read_cart_rom, NULL, address, rdword);
}

static void read_romd(void)
{
    readd(read_cart_rom, NULL, address, rdword);
}

static void write_rom(void)
{
    writew(write_cart_rom, NULL, address, word);
}


static inline uint32_t pif_ram_address(uint32_t address)
{
    return ((address & 0xfffc) - 0x7c0);
}

static int read_pif_ram(void* opaque, uint32_t address, uint32_t* value)
{
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08x", address);
        *value = 0;
        return -1;
    }

    memcpy(value, g_pif_ram + addr, sizeof(*value));
    *value = sl(*value);

    return 0;
}

static int write_pif_ram(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08x", address);
        return -1;
    }

    masked_write((uint32_t*)(&g_pif_ram[addr]), sl(value), sl(mask));

    if ((addr == 0x3c) && (mask & 0xff))
    {
        if (g_pif_ram[0x3f] == 0x08)
        {
            g_pif_ram[0x3f] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
        {
            update_pif_write();
        }
    }

    return 0;
}

static void read_pif(void)
{
    readw(read_pif_ram, NULL, address, rdword);
}

static void read_pifb(void)
{
    readb(read_pif_ram, NULL, address, rdword);
}

static void read_pifh(void)
{
    readh(read_pif_ram, NULL, address, rdword);
}

static void read_pifd(void)
{
    readd(read_pif_ram, NULL, address, rdword);
}

static void write_pif(void)
{
    writew(write_pif_ram, NULL, address, word);
}

static void write_pifb(void)
{
    writeb(write_pif_ram, NULL, address, cpu_byte);
}

static void write_pifh(void)
{
    writeh(write_pif_ram, NULL, address, hword);
}

static void write_pifd(void)
{
    writed(write_pif_ram, NULL, address, dword);
}

unsigned int *fast_mem_access(unsigned int address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */

    if ((address & 0xc0000000) != 0x80000000)
        address = virtual_to_physical_address(address, 2);

    address &= 0x1ffffffc;

    if (address < RDRAM_MAX_SIZE)
        return (unsigned int*)((unsigned char*)g_rdram + address);
    else if (address >= 0x10000000)
        return (unsigned int*)((unsigned char*)rom + address - 0x10000000);
    else if ((address & 0xffffe000) == 0x04000000)
        return (unsigned int*)((unsigned char*)g_sp_mem + (address & 0x1ffc));
    else
        return NULL;
}
