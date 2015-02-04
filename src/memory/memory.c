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

#include <assert.h>
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
static int read_nothing(uint32_t address, uint32_t* value);
static int write_nothing(uint32_t address, uint32_t value, uint32_t mask);
static int read_nomem(uint32_t address, uint32_t* value);
static int write_nomem(uint32_t address, uint32_t value, uint32_t mask);
static int read_rdram(uint32_t address, uint32_t* value);
static int write_rdram(uint32_t address, uint32_t value, uint32_t mask);
static int read_rdramFB(uint32_t address, uint32_t* value);
static int write_rdramFB(uint32_t address, uint32_t value, uint32_t mask);
static int read_rdramreg(uint32_t address, uint32_t* value);
static int write_rdramreg(uint32_t address, uint32_t value, uint32_t mask);
static int read_rsp_mem(uint32_t address, uint32_t* value);
static int write_rsp_mem(uint32_t address, uint32_t value, uint32_t mask);
static int read_rsp_reg(uint32_t address, uint32_t* value);
static int write_rsp_reg(uint32_t address, uint32_t value, uint32_t mask);
static int read_rsp(uint32_t address, uint32_t* value);
static int write_rsp(uint32_t address, uint32_t value, uint32_t mask);
static int read_dp(uint32_t address, uint32_t* value);
static int write_dp(uint32_t address, uint32_t value, uint32_t mask);
static int read_dps(uint32_t address, uint32_t* value);
static int write_dps(uint32_t address, uint32_t value, uint32_t mask);
static int read_mi(uint32_t address, uint32_t* value);
static int write_mi(uint32_t address, uint32_t value, uint32_t mask);
static int read_vi(uint32_t address, uint32_t* value);
static int write_vi(uint32_t address, uint32_t value, uint32_t mask);
static int read_ai(uint32_t address, uint32_t* value);
static int write_ai(uint32_t address, uint32_t value, uint32_t mask);
static int read_pi(uint32_t address, uint32_t* value);
static int write_pi(uint32_t address, uint32_t value, uint32_t mask);
static int read_ri(uint32_t address, uint32_t* value);
static int write_ri(uint32_t address, uint32_t value, uint32_t mask);
static int read_si(uint32_t address, uint32_t* value);
static int write_si(uint32_t address, uint32_t value, uint32_t mask);
static int read_flashram_status(uint32_t address, uint32_t* value);
static int write_flashram_dummy(uint32_t address, uint32_t value, uint32_t mask);
static int write_flashram_command(uint32_t address, uint32_t value, uint32_t mask);
static int read_rom(uint32_t address, uint32_t* value);
static int write_rom(uint32_t address, uint32_t value, uint32_t mask);
static int read_pif(uint32_t address, uint32_t* value);
static int write_pif(uint32_t address, uint32_t value, uint32_t mask);

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

/* hash tables of read/write functions */
static int (*readmem[0x10000])(uint32_t,uint32_t*);
static int (*writemem[0x10000])(uint32_t,uint32_t,uint32_t);

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

static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (*dst & ~mask) | (value & mask);
}

#ifdef DBG
static int memtype[0x10000];
static int (*saved_readmem[0x10000])(uint32_t,uint32_t*);
static int (*saved_writemem[0x10000])(uint32_t,uint32_t,uint32_t);

static int readmem_with_bp_checks(uint32_t address, uint32_t* value)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    return saved_readmem[address>>16](address, value);
}

static int writemem_with_bp_checks(uint32_t address, uint32_t value, uint32_t mask)
{
    check_breakpoints_on_mem_access((PC->addr)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    return saved_writemem[address>>16](address, value, mask);
}

void activate_memory_break_read(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_readmem[region] == NULL)
    {
        saved_readmem[region] = readmem[region];
        readmem[region] = readmem_with_bp_checks;
    }
}

void deactivate_memory_break_read(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_readmem[region] != NULL)
    {
        readmem[region] = saved_readmem[region];
        saved_readmem[region] = NULL;
    }
}

void activate_memory_break_write(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_writemem[region] == NULL)
    {
        saved_writemem[region] = writemem[region];
        writemem[region] = writemem_with_bp_checks;
    }
}

void deactivate_memory_break_write(uint32_t address)
{
    uint16_t region = address >> 16;

    if (saved_writemem[region] != NULL)
    {
        writemem[region] = saved_writemem[region];
        saved_writemem[region] = NULL;
    }
}

int get_memory_type(uint32_t address)
{
    return memtype[address >> 16];
}
#endif

#define R(x) read_ ## x
#define W(x) write_ ## x
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
        map_region(0xb000+i, M64P_MEM_ROM, R(rom), write_rom);
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

static void map_region_r(uint16_t region, int (*read32)(uint32_t,uint32_t*))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
    {
        saved_readmem[region] = read32;
        readmem[region] = readmem_with_bp_checks;
    }
    else
#endif
    {
        readmem[region] = read32;
    }
}

static void map_region_w(uint16_t region, int (*write32)(uint32_t,uint32_t,uint32_t))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
    {
        saved_writemem[region] = write32;
        writemem[region] = writemem_with_bp_checks;
    }
    else
#endif
    {
        writemem[region] = write32;
    }
}

void map_region(uint16_t region,
                int type,
                int (*read32)(uint32_t,uint32_t*),
                int (*write32)(uint32_t,uint32_t,uint32_t))
{
    map_region_t(region, type);
    map_region_r(region, read32);
    map_region_w(region, write32);
}

int read_aligned_word(uint32_t address, uint32_t* value)
{
    assert((address & 0x3) == 0);
    return readmem[address >> 16](address, value);
}

int write_aligned_word(uint32_t address, uint32_t value, uint32_t mask)
{
    assert((address & 0x3) == 0);
    return writemem[address >> 16](address, value, mask);
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


static int read_nothing(uint32_t address, uint32_t* value)
{
    if (address == 0xa5000508) *value = 0xFFFFFFFF;
    else *value = 0;

    return 0;
}

static int write_nothing(uint32_t address, uint32_t value, uint32_t mask)
{
    return 0;
}

static int read_nomem(uint32_t address, uint32_t* value)
{
    address = virtual_to_physical_address(address, 0);
    if (address == 0x00000000)
        return -1;

    return read_aligned_word(address, value);
}

static int write_nomem(uint32_t address, uint32_t value, uint32_t mask)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000)
        return - 1;

    return write_aligned_word(address, value, mask);
}


static inline uint32_t rdram_ram_address(uint32_t address)
{
    return (address & 0xffffff) >> 2;
}

static int read_rdram_ram(uint32_t address, uint32_t* value)
{
    uint32_t addr = rdram_ram_address(address);

    *value = g_rdram[addr];

    return 0;
}

static int write_rdram_ram(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = rdram_ram_address(address);

    masked_write(&g_rdram[addr], value, mask);

    return 0;
}

static int read_rdram(uint32_t address, uint32_t* value)
{
    return read_rdram_ram(address, value);
}

static int read_rdramFB(uint32_t address, uint32_t* value)
{
    pre_framebuffer_read(address);
    return read_rdram(address, value);
}

static int write_rdram(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rdram_ram(address, value, mask);
}

static int write_rdramFB(uint32_t address, uint32_t value, uint32_t mask)
{
    pre_framebuffer_write(address, 4);
    return write_rdram(address, value, mask);
}


static inline uint32_t rdram_reg(uint32_t address)
{
    return (address & 0x3ff) >> 2;
}

static int read_rdram_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = rdram_reg(address);

    *value = g_rdram_regs[reg];

    return 0;
}

static int write_rdram_regs(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rdram_reg(address);

    masked_write(&g_rdram_regs[reg], value, mask);

    return 0;
}

static int read_rdramreg(uint32_t address, uint32_t* value)
{
    return read_rdram_regs(address, value);
}

static int write_rdramreg(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rdram_regs(address, value, mask);
}


static inline uint32_t rsp_mem_address(uint32_t address)
{
    return (address & 0x1fff) >> 2;
}

static int read_rspmem(uint32_t address, uint32_t* value)
{
    uint32_t addr = rsp_mem_address(address);

    *value = g_sp_mem[addr];

    return 0;
}

static int write_rspmem(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = rsp_mem_address(address);

    masked_write(&g_sp_mem[addr], value, mask);

    return 0;
}

static int read_rsp_mem(uint32_t address, uint32_t* value)
{
    return read_rspmem(address, value);
}

static int write_rsp_mem(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rspmem(address, value, mask);
}


static inline uint32_t rsp_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_rsp_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg(address);

    *value = g_sp_regs[reg];

    if (reg == SP_SEMAPHORE_REG)
    {
        g_sp_regs[SP_SEMAPHORE_REG] = 1;
    }

    return 0;
}

static int write_rsp_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_rsp_reg(uint32_t address, uint32_t* value)
{
    return read_rsp_regs(address, value);
}

static int write_rsp_reg(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rsp_regs(address, value, mask);
}


static inline uint32_t rsp_reg2(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_rsp_regs2(uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg2(address);

    *value = g_sp_regs2[reg];

    return 0;
}

static int write_rsp_regs2(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rsp_reg2(address);

    masked_write(&g_sp_regs2[reg], value, mask);

    return 0;
}

static int read_rsp(uint32_t address, uint32_t* value)
{
    return read_rsp_regs2(address, value);
}

static int write_rsp(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rsp_regs2(address, value, mask);
}


static inline uint32_t dpc_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_dpc_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = dpc_reg(address);

    *value = g_dpc_regs[reg];

    return 0;
}

static int write_dpc_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_dp(uint32_t address, uint32_t* value)
{
    return read_dpc_regs(address, value);
}

static int write_dp(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_dpc_regs(address, value, mask);
}


static inline uint32_t dps_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_dps_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = dps_reg(address);

    *value = g_dps_regs[reg];

    return 0;
}

static int write_dps_regs(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = dps_reg(address);

    masked_write(&g_dps_regs[reg], value, mask);

    return 0;
}

static int read_dps(uint32_t address, uint32_t* value)
{
    return read_dps_regs(address, value);
}

static int write_dps(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_dps_regs(address, value, mask);
}


static inline uint32_t mi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_mi_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = mi_reg(address);

    *value = g_mi_regs[reg];

    return 0;
}

static int write_mi_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_mi(uint32_t address, uint32_t* value)
{
    return read_mi_regs(address, value);
}

static int write_mi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_mi_regs(address, value, mask);
}


static inline uint32_t vi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_vi_regs(uint32_t address, uint32_t* value)
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

static int write_vi_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_vi(uint32_t address, uint32_t* value)
{
    return read_vi_regs(address, value);
}

static int write_vi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_vi_regs(address, value, mask);
}


static inline uint32_t ai_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_ai_regs(uint32_t address, uint32_t* value)
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

static int write_ai_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_ai(uint32_t address, uint32_t* value)
{
    return read_ai_regs(address, value);
}

static int write_ai(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_ai_regs(address, value, mask);
}


static inline uint32_t pi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_pi_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = pi_reg(address);

    *value = g_pi_regs[reg];

    return 0;
}

static int write_pi_regs(uint32_t address, uint32_t value, uint32_t mask)
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


static int read_pi(uint32_t address, uint32_t* value)
{
    return read_pi_regs(address, value);
}

static int write_pi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_pi_regs(address, value, mask);
}


static inline uint32_t ri_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_ri_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = ri_reg(address);

    *value = g_ri_regs[reg];

    return 0;
}

static int write_ri_regs(uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = ri_reg(address);

    masked_write(&g_ri_regs[reg], value, mask);

    return 0;
}

static int read_ri(uint32_t address, uint32_t* value)
{
    return read_ri_regs(address, value);
}

static int write_ri(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_ri_regs(address, value, mask);
}


static inline uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

static int read_si_regs(uint32_t address, uint32_t* value)
{
    uint32_t reg = si_reg(address);

    *value = g_si_regs[reg];

    return 0;
}

static int write_si_regs(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_si(uint32_t address, uint32_t* value)
{
    return read_si_regs(address, value);
}

static int write_si(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_si_regs(address, value, mask);
}

static int read_flashram_status(uint32_t address, uint32_t* value)
{
    if (flashram_info.use_flashram != -1 && ((address & 0xffff) == 0))
    {
        *value = flashram_status();
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown read in read_flashram_status()");

    return 0;
}

static int write_flashram_dummy(uint32_t address, uint32_t value, uint32_t mask)
{
    return 0;
}

static int write_flashram_command(uint32_t address, uint32_t value, uint32_t mask)
{
    if (flashram_info.use_flashram != -1 && ((address & 0xffff) == 0))
    {
        flashram_command(value);
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown write in write_flashram_command()");

    return 0;
}


static unsigned int lastwrite = 0;

static inline uint32_t rom_address(uint32_t address)
{
    return (address & 0x03fffffc);
}

static int read_cart_rom(uint32_t address, uint32_t* value)
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

static int write_cart_rom(uint32_t address, uint32_t value, uint32_t mask)
{
    lastwrite = value & mask;

    return 0;
}


static int read_rom(uint32_t address, uint32_t* value)
{
    return read_cart_rom(address, value);
}

static int write_rom(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_cart_rom(address, value, mask);
}


static inline uint32_t pif_ram_address(uint32_t address)
{
    return ((address & 0xfffc) - 0x7c0);
}

static int read_pif_ram(uint32_t address, uint32_t* value)
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

static int write_pif_ram(uint32_t address, uint32_t value, uint32_t mask)
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

static int read_pif(uint32_t address, uint32_t* value)
{
    return read_pif_ram(address, value);
}

static int write_pif(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_pif_ram(address, value, mask);
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
