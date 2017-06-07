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

#include "memory.h"

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/ai/ai_controller.h"
#include "device/pi/pi_controller.h"
#include "device/r4300/new_dynarec/new_dynarec.h" /* for NEW_DYNAREC_ARM */
#include "device/r4300/r4300_core.h"
#include "device/rdp/rdp_core.h"
#include "device/ri/ri_controller.h"
#include "device/rsp/rsp_core.h"
#include "device/si/si_controller.h"
#include "device/vi/vi_controller.h"
#include "main/main.h"

#ifdef DBG
#include <string.h>

#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_types.h"
#endif

#include <stddef.h>
#include <stdint.h>

typedef void (*readfn)(void*,uint32_t,uint32_t*);
typedef void (*writefn)(void*,uint32_t,uint32_t,uint32_t);


static void readw(readfn read_word, void* opaque, uint32_t address, uint64_t* value)
{
    uint32_t w;
    read_word(opaque, (address & ~0x3), &w);
    *value = w;
}

static void writew(writefn write_word, void* opaque, uint32_t address, uint32_t value, uint32_t wmask)
{
    write_word(opaque, (address & ~0x3), value, wmask);
}

/* XXX: temporary helper function */
static void* get_opaque(void)
{
    return g_dev.mem.opaque[*r4300_address(&g_dev.r4300) >> 16];
}

static void read_nothing(void)
{
    *g_dev.r4300.rdword = *r4300_address(&g_dev.r4300) & 0xFFFF;
    *g_dev.r4300.rdword = (*g_dev.r4300.rdword << 16) | *g_dev.r4300.rdword;
}

static void write_nothing(void)
{
}

static void read_nomem(void)
{
    struct r4300_core* r4300 = (struct r4300_core*)get_opaque();

    *r4300_address(r4300) = virtual_to_physical_address(r4300, *r4300_address(r4300), 0);
    if (*r4300_address(r4300) == 0x00000000) return;
    read_word_in_memory();
}

static void write_nomem(void)
{
    struct r4300_core* r4300 = (struct r4300_core*)get_opaque();

    invalidate_r4300_cached_code(r4300, *r4300_address(r4300), 4);
    *r4300_address(r4300) = virtual_to_physical_address(r4300, *r4300_address(r4300),1);
    if (*r4300_address(r4300) == 0x00000000) return;
    write_word_in_memory();
}

void read_rdram(void)
{
    readw(read_rdram_dram, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

void write_rdram(void)
{
    writew(write_rdram_dram, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


void read_rdramFB(void)
{
    readw(read_rdram_fb, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

void write_rdramFB(void)
{
    writew(write_rdram_fb, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_rdramreg(void)
{
    readw(read_rdram_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_rdramreg(void)
{
    writew(write_rdram_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_rspmem(void)
{
    readw(read_rsp_mem, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_rspmem(void)
{
    writew(write_rsp_mem, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_rspreg(void)
{
    readw(read_rsp_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_rspreg(void)
{
    writew(write_rsp_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_rspreg2(void)
{
    readw(read_rsp_regs2, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_rspreg2(void)
{
    writew(write_rsp_regs2, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_dp(void)
{
    readw(read_dpc_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_dp(void)
{
    writew(write_dpc_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_dps(void)
{
    readw(read_dps_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_dps(void)
{
    writew(write_dps_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_mi(void)
{
    readw(read_mi_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

void write_mi(void)
{
    writew(write_mi_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_vi(void)
{
    readw(read_vi_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_vi(void)
{
    writew(write_vi_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_ai(void)
{
    readw(read_ai_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_ai(void)
{
    writew(write_ai_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_pi(void)
{
    readw(read_pi_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_pi(void)
{
    writew(write_pi_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_ri(void)
{
    readw(read_ri_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_ri(void)
{
    writew(write_ri_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_si(void)
{
    readw(read_si_regs, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_si(void)
{
    writew(write_si_regs, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}

static void read_pi_flashram_status(void)
{
    readw(read_flashram_status, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_pi_flashram_status(void)
{
    writew(write_flashram_status, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}

static void read_pi_flashram_command(void)
{
    readw(read_flashram_command, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_pi_flashram_command(void)
{
    writew(write_flashram_command, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_rom(void)
{
    readw(read_cart_rom, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_rom(void)
{
    writew(write_cart_rom, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}


static void read_pif(void)
{
    readw(read_pif_ram, get_opaque(), *r4300_address(&g_dev.r4300), g_dev.r4300.rdword);
}

static void write_pif(void)
{
    writew(write_pif_ram, get_opaque(), *r4300_address(&g_dev.r4300), *r4300_wword(&g_dev.r4300), *r4300_wmask(&g_dev.r4300));
}

#ifdef DBG
static void readmem_with_bp_checks(void)
{
    struct r4300_core* r4300 = (struct r4300_core*)get_opaque();

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, *r4300_address(r4300), 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    r4300->mem->saved_readmem[*r4300_address(r4300)>>16]();
}

static void writemem_with_bp_checks(void)
{
    struct r4300_core* r4300 = (struct r4300_core*)get_opaque();

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, *r4300_address(r4300), 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    r4300->mem->saved_writemem[*r4300_address(r4300)>>16]();
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_readmem[region] == NULL)
    {
        /* only change opaque value if memory_break_write is not active */
        if (mem->saved_writemem[region] == NULL) {
            mem->saved_opaque[region] = mem->opaque[region];
            mem->opaque[region] = &g_dev.r4300;
        }

        mem->saved_readmem[region] = mem->readmem[region];
        mem->readmem[region] = readmem_with_bp_checks;
    }
}

void deactivate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_readmem[region] != NULL)
    {
        /* only restore opaque value if memory_break_write is not active */
        if (mem->saved_writemem[region] == NULL) {
            mem->opaque[region] = mem->saved_opaque[region];
            mem->saved_opaque[region] = NULL;
        }

        mem->readmem[region] = mem->saved_readmem[region];
        mem->saved_readmem[region] = NULL;
    }
}

void activate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_writemem[region] == NULL)
    {
        /* only change opaque value if memory_break_read is not active */
        if (mem->saved_readmem[region] == NULL) {
            mem->saved_opaque[region] = mem->opaque[region];
            mem->opaque[region] = &g_dev.r4300;
        }

        mem->saved_writemem[region] = mem->writemem[region];
        mem->writemem[region] = writemem_with_bp_checks;
    }
}

void deactivate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_writemem[region] != NULL)
    {
        /* only restore opaque value if memory_break_read is not active */
        if (mem->saved_readmem[region] == NULL) {
            mem->opaque[region] = mem->saved_opaque[region];
            mem->saved_opaque[region] = NULL;
        }

        mem->writemem[region] = mem->saved_writemem[region];
        mem->saved_writemem[region] = NULL;
    }
}

int get_memory_type(struct memory* mem, uint32_t address)
{
    return mem->memtype[address >> 16];
}
#endif

#define R(x) read_ ## x
#define W(x) write_ ## x
#define RW(x) R(x), W(x)

void poweron_memory(struct memory* mem)
{
    int i;

#ifdef DBG
    memset(mem->saved_opaque, 0, 0x10000*sizeof(mem->saved_opaque[0]));
    memset(mem->saved_readmem, 0, 0x10000*sizeof(mem->saved_readmem[0]));
    memset(mem->saved_writemem, 0, 0x10000*sizeof(mem->saved_writemem[0]));
#endif

    /* clear mappings */
    for(i = 0; i < 0x10000; ++i)
    {
        map_region(mem, i, M64P_MEM_NOMEM, &g_dev.r4300, RW(nomem));
    }

    /* map RDRAM */
    for(i = 0; i < /*0x40*/0x80; ++i)
    {
        map_region(mem, 0x8000+i, M64P_MEM_RDRAM, &g_dev.ri, RW(rdram));
        map_region(mem, 0xa000+i, M64P_MEM_RDRAM, &g_dev.ri, RW(rdram));
    }
    for(i = /*0x40*/0x80; i < 0x3f0; ++i)
    {
        map_region(mem, 0x8000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map RDRAM registers */
    map_region(mem, 0x83f0, M64P_MEM_RDRAMREG, &g_dev.ri, RW(rdramreg));
    map_region(mem, 0xa3f0, M64P_MEM_RDRAMREG, &g_dev.ri, RW(rdramreg));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x83f0+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa3f0+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map RSP memory */
    map_region(mem, 0x8400, M64P_MEM_RSPMEM, &g_dev.sp, RW(rspmem));
    map_region(mem, 0xa400, M64P_MEM_RSPMEM, &g_dev.sp, RW(rspmem));
    for(i = 1; i < 0x4; ++i)
    {
        map_region(mem, 0x8400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map RSP registers (1) */
    map_region(mem, 0x8404, M64P_MEM_RSPREG, &g_dev.sp, RW(rspreg));
    map_region(mem, 0xa404, M64P_MEM_RSPREG, &g_dev.sp, RW(rspreg));
    for(i = 0x5; i < 0x8; ++i)
    {
        map_region(mem, 0x8400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map RSP registers (2) */
    map_region(mem, 0x8408, M64P_MEM_RSP, &g_dev.sp, RW(rspreg2));
    map_region(mem, 0xa408, M64P_MEM_RSP, &g_dev.sp, RW(rspreg2));
    for(i = 0x9; i < 0x10; ++i)
    {
        map_region(mem, 0x8400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa400+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map DPC registers */
    map_region(mem, 0x8410, M64P_MEM_DP, &g_dev.dp, RW(dp));
    map_region(mem, 0xa410, M64P_MEM_DP, &g_dev.dp, RW(dp));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8410+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa410+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map DPS registers */
    map_region(mem, 0x8420, M64P_MEM_DPS, &g_dev.dp, RW(dps));
    map_region(mem, 0xa420, M64P_MEM_DPS, &g_dev.dp, RW(dps));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8420+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa420+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map MI registers */
    map_region(mem, 0x8430, M64P_MEM_MI, &g_dev.r4300, RW(mi));
    map_region(mem, 0xa430, M64P_MEM_MI, &g_dev.r4300, RW(mi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8430+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa430+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map VI registers */
    map_region(mem, 0x8440, M64P_MEM_VI, &g_dev.vi, RW(vi));
    map_region(mem, 0xa440, M64P_MEM_VI, &g_dev.vi, RW(vi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8440+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa440+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map AI registers */
    map_region(mem, 0x8450, M64P_MEM_AI, &g_dev.ai, RW(ai));
    map_region(mem, 0xa450, M64P_MEM_AI, &g_dev.ai, RW(ai));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8450+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa450+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map PI registers */
    map_region(mem, 0x8460, M64P_MEM_PI, &g_dev.pi, RW(pi));
    map_region(mem, 0xa460, M64P_MEM_PI, &g_dev.pi, RW(pi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8460+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa460+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map RI registers */
    map_region(mem, 0x8470, M64P_MEM_RI, &g_dev.ri, RW(ri));
    map_region(mem, 0xa470, M64P_MEM_RI, &g_dev.ri, RW(ri));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(mem, 0x8470+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa470+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map SI registers */
    map_region(mem, 0x8480, M64P_MEM_SI, &g_dev.si, RW(si));
    map_region(mem, 0xa480, M64P_MEM_SI, &g_dev.si, RW(si));
    for(i = 0x481; i < 0x500; ++i)
    {
        map_region(mem, 0x8000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    for(i = 0x500; i < 0x800; ++i)
    {
        map_region(mem, 0x8000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map flashram/sram */
    map_region(mem, 0x8800, M64P_MEM_FLASHRAMSTAT, &g_dev.pi, RW(pi_flashram_status));
    map_region(mem, 0xa800, M64P_MEM_FLASHRAMSTAT, &g_dev.pi, RW(pi_flashram_status));
    map_region(mem, 0x8801, M64P_MEM_NOTHING, &g_dev.pi, RW(pi_flashram_command));
    map_region(mem, 0xa801, M64P_MEM_NOTHING, &g_dev.pi, RW(pi_flashram_command));
    for(i = 0x802; i < 0x1000; ++i)
    {
        map_region(mem, 0x8000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xa000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map cart ROM */
    for(i = 0; i < (g_dev.pi.cart_rom.rom_size >> 16); ++i)
    {
        map_region(mem, 0x9000+i, M64P_MEM_ROM, &g_dev.pi, R(rom), W(nothing));
        map_region(mem, 0xb000+i, M64P_MEM_ROM, &g_dev.pi, R(rom), W(rom));
    }
    for(i = (g_dev.pi.cart_rom.rom_size >> 16); i < 0xfc0; ++i)
    {
        map_region(mem, 0x9000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xb000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }

    /* map PIF RAM */
    map_region(mem, 0x9fc0, M64P_MEM_PIF, &g_dev.si, RW(pif));
    map_region(mem, 0xbfc0, M64P_MEM_PIF, &g_dev.si, RW(pif));
    for(i = 0xfc1; i < 0x1000; ++i)
    {
        map_region(mem, 0x9000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
        map_region(mem, 0xb000+i, M64P_MEM_NOTHING, &g_dev.r4300, RW(nothing));
    }
}

static void map_region_t(struct memory* mem, uint16_t region, int type)
{
#ifdef DBG
    mem->memtype[region] = type;
#else
    (void)region;
    (void)type;
#endif
}

static void map_region_o(struct memory* mem,
        uint16_t region,
        void* opaque)
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED) != -1)
    {
        mem->saved_opaque[region] = opaque;
        mem->opaque[region] = &g_dev.r4300;
    }
    else
#endif
    {
        mem->opaque[region] = opaque;
    }
}

static void map_region_r(struct memory* mem,
        uint16_t region,
        void (*read32)(void))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
    {
        mem->saved_readmem [region] = read32;
        mem->readmem [region] = readmem_with_bp_checks;
    }
    else
#endif
    {
        mem->readmem [region] = read32;
    }
}

static void map_region_w(struct memory* mem,
        uint16_t region,
        void (*write32)(void))
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
    {
        mem->saved_writemem [region] = write32;
        mem->writemem [region] = writemem_with_bp_checks;
    }
    else
#endif
    {
        mem->writemem [region] = write32;
    }
}

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                void* opaque,
                void (*read32)(void),
                void (*write32)(void))
{
    map_region_t(mem, region, type);
    map_region_o(mem, region, opaque);
    map_region_r(mem, region, read32);
    map_region_w(mem, region, write32);
}

uint32_t *fast_mem_access(uint32_t address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */

    if ((address & UINT32_C(0xc0000000)) != UINT32_C(0x80000000))
        address = virtual_to_physical_address(&g_dev.r4300, address, 2);

    address &= UINT32_C(0x1ffffffc);

    if (address < RDRAM_MAX_SIZE)
        return (uint32_t*) ((uint8_t*) g_dev.ri.rdram.dram + address);
    else if (address >= UINT32_C(0x10000000))
        return (uint32_t*) ((uint8_t*) g_dev.pi.cart_rom.rom + (address - UINT32_C(0x10000000)));
    else if ((address & UINT32_C(0xffffe000)) == UINT32_C(0x04000000))
        return (uint32_t*) ((uint8_t*) g_dev.sp.mem + (address & UINT32_C(0x1ffc)));
    else
        return NULL;
}
