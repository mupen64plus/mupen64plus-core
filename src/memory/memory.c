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
unsigned int PIF_RAM[0x40/4];
unsigned char *PIF_RAMb = (unsigned char *)(PIF_RAM);

#if NEW_DYNAREC != NEW_DYNAREC_ARM
// address : address of the read/write operation being done
unsigned int address = 0;
#endif
// *address_low = the lower 16 bit of the address :
#ifdef M64P_BIG_ENDIAN
static unsigned short *address_low = (unsigned short *)(&address)+1;
#else
static unsigned short *address_low = (unsigned short *)(&address);
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

typedef int (*readfn)(uint32_t,uint32_t*);
typedef int (*writefn)(uint32_t,uint32_t,uint32_t);

static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (*dst & ~mask) | (value & mask);
}

static inline unsigned int bshift(uint32_t address)
{
    return ((address & 3) ^ 3) << 3;
}

static inline unsigned int hshift(uint32_t address)
{
    return ((address & 2) ^ 2) << 3;
}

static int readb(readfn read_word, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    unsigned shift = bshift(address);
    int result = read_word(address, &w);
    *value = (w >> shift) & 0xff;

    return result;
}

static int readh(readfn read_word, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    unsigned shift = hshift(address);
    int result = read_word(address, &w);
    *value = (w >> shift) & 0xffff;

    return result;
}

static int readw(readfn read_word, uint32_t address, unsigned long long int* value)
{
    uint32_t w;
    int result = read_word(address, &w);
    *value = w;

    return result;
}

static int readd(readfn read_word, uint32_t address, unsigned long long int* value)
{
    uint32_t w[2];
    int result =
    read_word(address    , &w[0]);
    read_word(address + 4, &w[1]);
    *value = ((uint64_t)w[0] << 32) | w[1];

    return result;
}

static int writeb(writefn write_word, uint32_t address, uint8_t value)
{
    unsigned int shift = bshift(address);
    uint32_t w = (uint32_t)value << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    return write_word(address, w, mask);
}

static int writeh(writefn write_word, uint32_t address, uint16_t value)
{
    unsigned int shift = hshift(address);
    uint32_t w = (uint32_t)value << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    return write_word(address, w, mask);
}

static int writew(writefn write_word, uint32_t address, uint32_t value)
{
    return write_word(address, value, ~0U);
}

static int writed(writefn write_word, uint32_t address, uint64_t value)
{
    int result =
    write_word(address    , value >> 32, ~0U);
    write_word(address + 4, value      , ~0U);

    return result;
}


int init_memory(void)
{
    int i;

    //init hash tables
    for (i=0; i<(0x10000); i++)
    {
        readmem[i] = read_nomem;
        readmemb[i] = read_nomemb;
        readmemd[i] = read_nomemd;
        readmemh[i] = read_nomemh;
        writemem[i] = write_nomem;
        writememb[i] = write_nomemb;
        writememd[i] = write_nomemd;
        writememh[i] = write_nomemh;
    }

    /* init RDRAM */
    memset(g_rdram, 0, RDRAM_MAX_SIZE);

    for (i=0; i</*0x40*/0x80; i++)
    {
        readmem[(0x8000+i)] = read_rdram;
        readmem[(0xa000+i)] = read_rdram;
        readmemb[(0x8000+i)] = read_rdramb;
        readmemb[(0xa000+i)] = read_rdramb;
        readmemh[(0x8000+i)] = read_rdramh;
        readmemh[(0xa000+i)] = read_rdramh;
        readmemd[(0x8000+i)] = read_rdramd;
        readmemd[(0xa000+i)] = read_rdramd;
        writemem[(0x8000+i)] = write_rdram;
        writemem[(0xa000+i)] = write_rdram;
        writememb[(0x8000+i)] = write_rdramb;
        writememb[(0xa000+i)] = write_rdramb;
        writememh[(0x8000+i)] = write_rdramh;
        writememh[(0xa000+i)] = write_rdramh;
        writememd[(0x8000+i)] = write_rdramd;
        writememd[(0xa000+i)] = write_rdramd;
    }

    for (i=/*0x40*/0x80; i<0x3F0; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    /* init RDRAM registers */
    memset(g_rdram_regs, 0, RDRAM_REGS_COUNT*sizeof(g_rdram_regs[0]));

    readmem[0x83f0] = read_rdramreg;
    readmem[0xa3f0] = read_rdramreg;
    readmemb[0x83f0] = read_rdramregb;
    readmemb[0xa3f0] = read_rdramregb;
    readmemh[0x83f0] = read_rdramregh;
    readmemh[0xa3f0] = read_rdramregh;
    readmemd[0x83f0] = read_rdramregd;
    readmemd[0xa3f0] = read_rdramregd;
    writemem[0x83f0] = write_rdramreg;
    writemem[0xa3f0] = write_rdramreg;
    writememb[0x83f0] = write_rdramregb;
    writememb[0xa3f0] = write_rdramregb;
    writememh[0x83f0] = write_rdramregh;
    writememh[0xa3f0] = write_rdramregh;
    writememd[0x83f0] = write_rdramregd;
    writememd[0xa3f0] = write_rdramregd;

    for (i=1; i<0x10; i++)
    {
        readmem[0x83f0+i] = read_nothing;
        readmem[0xa3f0+i] = read_nothing;
        readmemb[0x83f0+i] = read_nothingb;
        readmemb[0xa3f0+i] = read_nothingb;
        readmemh[0x83f0+i] = read_nothingh;
        readmemh[0xa3f0+i] = read_nothingh;
        readmemd[0x83f0+i] = read_nothingd;
        readmemd[0xa3f0+i] = read_nothingd;
        writemem[0x83f0+i] = write_nothing;
        writemem[0xa3f0+i] = write_nothing;
        writememb[0x83f0+i] = write_nothingb;
        writememb[0xa3f0+i] = write_nothingb;
        writememh[0x83f0+i] = write_nothingh;
        writememh[0xa3f0+i] = write_nothingh;
        writememd[0x83f0+i] = write_nothingd;
        writememd[0xa3f0+i] = write_nothingd;
    }

    /* init RSP memory */
    memset(g_sp_mem, 0, SP_MEM_SIZE);

    readmem[0x8400] = read_rsp_mem;
    readmem[0xa400] = read_rsp_mem;
    readmemb[0x8400] = read_rsp_memb;
    readmemb[0xa400] = read_rsp_memb;
    readmemh[0x8400] = read_rsp_memh;
    readmemh[0xa400] = read_rsp_memh;
    readmemd[0x8400] = read_rsp_memd;
    readmemd[0xa400] = read_rsp_memd;
    writemem[0x8400] = write_rsp_mem;
    writemem[0xa400] = write_rsp_mem;
    writememb[0x8400] = write_rsp_memb;
    writememb[0xa400] = write_rsp_memb;
    writememh[0x8400] = write_rsp_memh;
    writememh[0xa400] = write_rsp_memh;
    writememd[0x8400] = write_rsp_memd;
    writememd[0xa400] = write_rsp_memd;

    for (i=1; i<0x4; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    /* init RSP registers */
    memset(g_sp_regs, 0, SP_REGS_COUNT*sizeof(g_sp_regs[0]));
    g_sp_regs[SP_STATUS_REG] = 1;

    readmem[0x8404] = read_rsp_reg;
    readmem[0xa404] = read_rsp_reg;
    readmemb[0x8404] = read_rsp_regb;
    readmemb[0xa404] = read_rsp_regb;
    readmemh[0x8404] = read_rsp_regh;
    readmemh[0xa404] = read_rsp_regh;
    readmemd[0x8404] = read_rsp_regd;
    readmemd[0xa404] = read_rsp_regd;
    writemem[0x8404] = write_rsp_reg;
    writemem[0xa404] = write_rsp_reg;
    writememb[0x8404] = write_rsp_regb;
    writememb[0xa404] = write_rsp_regb;
    writememh[0x8404] = write_rsp_regh;
    writememh[0xa404] = write_rsp_regh;
    writememd[0x8404] = write_rsp_regd;
    writememd[0xa404] = write_rsp_regd;

    for (i=5; i<8; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    /* init RSP registers (2) */
    memset(g_sp_regs2, 0, SP_REGS2_COUNT*sizeof(g_sp_regs2[0]));

    readmem[0x8408] = read_rsp;
    readmem[0xa408] = read_rsp;
    readmemb[0x8408] = read_rspb;
    readmemb[0xa408] = read_rspb;
    readmemh[0x8408] = read_rsph;
    readmemh[0xa408] = read_rsph;
    readmemd[0x8408] = read_rspd;
    readmemd[0xa408] = read_rspd;
    writemem[0x8408] = write_rsp;
    writemem[0xa408] = write_rsp;
    writememb[0x8408] = write_rspb;
    writememb[0xa408] = write_rspb;
    writememh[0x8408] = write_rsph;
    writememh[0xa408] = write_rsph;
    writememd[0x8408] = write_rspd;
    writememd[0xa408] = write_rspd;
    
    for (i=9; i<0x10; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    /* init DPC registers */
    memset(g_dpc_regs, 0, DPC_REGS_COUNT*sizeof(g_dpc_regs[0]));

    readmem[0x8410] = read_dp;
    readmem[0xa410] = read_dp;
    readmemb[0x8410] = read_dpb;
    readmemb[0xa410] = read_dpb;
    readmemh[0x8410] = read_dph;
    readmemh[0xa410] = read_dph;
    readmemd[0x8410] = read_dpd;
    readmemd[0xa410] = read_dpd;
    writemem[0x8410] = write_dp;
    writemem[0xa410] = write_dp;
    writememb[0x8410] = write_dpb;
    writememb[0xa410] = write_dpb;
    writememh[0x8410] = write_dph;
    writememh[0xa410] = write_dph;
    writememd[0x8410] = write_dpd;
    writememd[0xa410] = write_dpd;

    for (i=1; i<0x10; i++)
    {
        readmem[0x8410+i] = read_nothing;
        readmem[0xa410+i] = read_nothing;
        readmemb[0x8410+i] = read_nothingb;
        readmemb[0xa410+i] = read_nothingb;
        readmemh[0x8410+i] = read_nothingh;
        readmemh[0xa410+i] = read_nothingh;
        readmemd[0x8410+i] = read_nothingd;
        readmemd[0xa410+i] = read_nothingd;
        writemem[0x8410+i] = write_nothing;
        writemem[0xa410+i] = write_nothing;
        writememb[0x8410+i] = write_nothingb;
        writememb[0xa410+i] = write_nothingb;
        writememh[0x8410+i] = write_nothingh;
        writememh[0xa410+i] = write_nothingh;
        writememd[0x8410+i] = write_nothingd;
        writememd[0xa410+i] = write_nothingd;
    }

    /* init DPS registers */
    memset(g_dps_regs, 0, DPS_REGS_COUNT*sizeof(g_dps_regs[0]));

    readmem[0x8420] = read_dps;
    readmem[0xa420] = read_dps;
    readmemb[0x8420] = read_dpsb;
    readmemb[0xa420] = read_dpsb;
    readmemh[0x8420] = read_dpsh;
    readmemh[0xa420] = read_dpsh;
    readmemd[0x8420] = read_dpsd;
    readmemd[0xa420] = read_dpsd;
    writemem[0x8420] = write_dps;
    writemem[0xa420] = write_dps;
    writememb[0x8420] = write_dpsb;
    writememb[0xa420] = write_dpsb;
    writememh[0x8420] = write_dpsh;
    writememh[0xa420] = write_dpsh;
    writememd[0x8420] = write_dpsd;
    writememd[0xa420] = write_dpsd;
    
    for (i=1; i<0x10; i++)
    {
        readmem[0x8420+i] = read_nothing;
        readmem[0xa420+i] = read_nothing;
        readmemb[0x8420+i] = read_nothingb;
        readmemb[0xa420+i] = read_nothingb;
        readmemh[0x8420+i] = read_nothingh;
        readmemh[0xa420+i] = read_nothingh;
        readmemd[0x8420+i] = read_nothingd;
        readmemd[0xa420+i] = read_nothingd;
        writemem[0x8420+i] = write_nothing;
        writemem[0xa420+i] = write_nothing;
        writememb[0x8420+i] = write_nothingb;
        writememb[0xa420+i] = write_nothingb;
        writememh[0x8420+i] = write_nothingh;
        writememh[0xa420+i] = write_nothingh;
        writememd[0x8420+i] = write_nothingd;
        writememd[0xa420+i] = write_nothingd;
    }

    /* init MI registers */
    memset(g_mi_regs, 0, MI_REGS_COUNT*sizeof(g_mi_regs[0]));
    g_mi_regs[MI_VERSION_REG] = 0x02020102;

    readmem[0xa830] = read_mi;
    readmem[0xa430] = read_mi;
    readmemb[0xa830] = read_mib;
    readmemb[0xa430] = read_mib;
    readmemh[0xa830] = read_mih;
    readmemh[0xa430] = read_mih;
    readmemd[0xa830] = read_mid;
    readmemd[0xa430] = read_mid;
    writemem[0x8430] = write_mi;
    writemem[0xa430] = write_mi;
    writememb[0x8430] = write_mib;
    writememb[0xa430] = write_mib;
    writememh[0x8430] = write_mih;
    writememh[0xa430] = write_mih;
    writememd[0x8430] = write_mid;
    writememd[0xa430] = write_mid;
    
    for (i=1; i<0x10; i++)
    {
        readmem[0x8430+i] = read_nothing;
        readmem[0xa430+i] = read_nothing;
        readmemb[0x8430+i] = read_nothingb;
        readmemb[0xa430+i] = read_nothingb;
        readmemh[0x8430+i] = read_nothingh;
        readmemh[0xa430+i] = read_nothingh;
        readmemd[0x8430+i] = read_nothingd;
        readmemd[0xa430+i] = read_nothingd;
        writemem[0x8430+i] = write_nothing;
        writemem[0xa430+i] = write_nothing;
        writememb[0x8430+i] = write_nothingb;
        writememb[0xa430+i] = write_nothingb;
        writememh[0x8430+i] = write_nothingh;
        writememh[0xa430+i] = write_nothingh;
        writememd[0x8430+i] = write_nothingd;
        writememd[0xa430+i] = write_nothingd;
    }

    /* init VI registers */
    memset(g_vi_regs, 0, VI_REGS_COUNT*sizeof(g_vi_regs[0]));

    readmem[0x8440] = read_vi;
    readmem[0xa440] = read_vi;
    readmemb[0x8440] = read_vib;
    readmemb[0xa440] = read_vib;
    readmemh[0x8440] = read_vih;
    readmemh[0xa440] = read_vih;
    readmemd[0x8440] = read_vid;
    readmemd[0xa440] = read_vid;
    writemem[0x8440] = write_vi;
    writemem[0xa440] = write_vi;
    writememb[0x8440] = write_vib;
    writememb[0xa440] = write_vib;
    writememh[0x8440] = write_vih;
    writememh[0xa440] = write_vih;
    writememd[0x8440] = write_vid;
    writememd[0xa440] = write_vid;

    for (i=1; i<0x10; i++)
    {
        readmem[0x8440+i] = read_nothing;
        readmem[0xa440+i] = read_nothing;
        readmemb[0x8440+i] = read_nothingb;
        readmemb[0xa440+i] = read_nothingb;
        readmemh[0x8440+i] = read_nothingh;
        readmemh[0xa440+i] = read_nothingh;
        readmemd[0x8440+i] = read_nothingd;
        readmemd[0xa440+i] = read_nothingd;
        writemem[0x8440+i] = write_nothing;
        writemem[0xa440+i] = write_nothing;
        writememb[0x8440+i] = write_nothingb;
        writememb[0xa440+i] = write_nothingb;
        writememh[0x8440+i] = write_nothingh;
        writememh[0xa440+i] = write_nothingh;
        writememd[0x8440+i] = write_nothingd;
        writememd[0xa440+i] = write_nothingd;
    }

    /* init AI registers */
    memset(g_ai_regs, 0, AI_REGS_COUNT*sizeof(g_ai_regs[0]));
    memset(g_ai_fifo, 0, 2*sizeof(g_ai_fifo[0]));

    readmem[0x8450] = read_ai;
    readmem[0xa450] = read_ai;
    readmemb[0x8450] = read_aib;
    readmemb[0xa450] = read_aib;
    readmemh[0x8450] = read_aih;
    readmemh[0xa450] = read_aih;
    readmemd[0x8450] = read_aid;
    readmemd[0xa450] = read_aid;
    writemem[0x8450] = write_ai;
    writemem[0xa450] = write_ai;
    writememb[0x8450] = write_aib;
    writememb[0xa450] = write_aib;
    writememh[0x8450] = write_aih;
    writememh[0xa450] = write_aih;
    writememd[0x8450] = write_aid;
    writememd[0xa450] = write_aid;

    for (i=1; i<0x10; i++)
    {
        readmem[0x8450+i] = read_nothing;
        readmem[0xa450+i] = read_nothing;
        readmemb[0x8450+i] = read_nothingb;
        readmemb[0xa450+i] = read_nothingb;
        readmemh[0x8450+i] = read_nothingh;
        readmemh[0xa450+i] = read_nothingh;
        readmemd[0x8450+i] = read_nothingd;
        readmemd[0xa450+i] = read_nothingd;
        writemem[0x8450+i] = write_nothing;
        writemem[0xa450+i] = write_nothing;
        writememb[0x8450+i] = write_nothingb;
        writememb[0xa450+i] = write_nothingb;
        writememh[0x8450+i] = write_nothingh;
        writememh[0xa450+i] = write_nothingh;
        writememd[0x8450+i] = write_nothingd;
        writememd[0xa450+i] = write_nothingd;
    }

    /* init PI registers */
    memset(g_pi_regs, 0, PI_REGS_COUNT*sizeof(g_pi_regs[0]));

    readmem[0x8460] = read_pi;
    readmem[0xa460] = read_pi;
    readmemb[0x8460] = read_pib;
    readmemb[0xa460] = read_pib;
    readmemh[0x8460] = read_pih;
    readmemh[0xa460] = read_pih;
    readmemd[0x8460] = read_pid;
    readmemd[0xa460] = read_pid;
    writemem[0x8460] = write_pi;
    writemem[0xa460] = write_pi;
    writememb[0x8460] = write_pib;
    writememb[0xa460] = write_pib;
    writememh[0x8460] = write_pih;
    writememh[0xa460] = write_pih;
    writememd[0x8460] = write_pid;
    writememd[0xa460] = write_pid;

    for (i=1; i<0x10; i++)
    {
        readmem[0x8460+i] = read_nothing;
        readmem[0xa460+i] = read_nothing;
        readmemb[0x8460+i] = read_nothingb;
        readmemb[0xa460+i] = read_nothingb;
        readmemh[0x8460+i] = read_nothingh;
        readmemh[0xa460+i] = read_nothingh;
        readmemd[0x8460+i] = read_nothingd;
        readmemd[0xa460+i] = read_nothingd;
        writemem[0x8460+i] = write_nothing;
        writemem[0xa460+i] = write_nothing;
        writememb[0x8460+i] = write_nothingb;
        writememb[0xa460+i] = write_nothingb;
        writememh[0x8460+i] = write_nothingh;
        writememh[0xa460+i] = write_nothingh;
        writememd[0x8460+i] = write_nothingd;
        writememd[0xa460+i] = write_nothingd;
    }

    /* init RI registers */
    memset(g_ri_regs, 0, RI_REGS_COUNT*sizeof(g_ri_regs[0]));

    readmem[0x8470] = read_ri;
    readmem[0xa470] = read_ri;
    readmemb[0x8470] = read_rib;
    readmemb[0xa470] = read_rib;
    readmemh[0x8470] = read_rih;
    readmemh[0xa470] = read_rih;
    readmemd[0x8470] = read_rid;
    readmemd[0xa470] = read_rid;
    writemem[0x8470] = write_ri;
    writemem[0xa470] = write_ri;
    writememb[0x8470] = write_rib;
    writememb[0xa470] = write_rib;
    writememh[0x8470] = write_rih;
    writememh[0xa470] = write_rih;
    writememd[0x8470] = write_rid;
    writememd[0xa470] = write_rid;

    for (i=1; i<0x10; i++)
    {
        readmem[0x8470+i] = read_nothing;
        readmem[0xa470+i] = read_nothing;
        readmemb[0x8470+i] = read_nothingb;
        readmemb[0xa470+i] = read_nothingb;
        readmemh[0x8470+i] = read_nothingh;
        readmemh[0xa470+i] = read_nothingh;
        readmemd[0x8470+i] = read_nothingd;
        readmemd[0xa470+i] = read_nothingd;
        writemem[0x8470+i] = write_nothing;
        writemem[0xa470+i] = write_nothing;
        writememb[0x8470+i] = write_nothingb;
        writememb[0xa470+i] = write_nothingb;
        writememh[0x8470+i] = write_nothingh;
        writememh[0xa470+i] = write_nothingh;
        writememd[0x8470+i] = write_nothingd;
        writememd[0xa470+i] = write_nothingd;
    }

    /* init SI registers */
    memset(g_si_regs, 0, SI_REGS_COUNT*sizeof(g_si_regs[0]));

    readmem[0x8480] = read_si;
    readmem[0xa480] = read_si;
    readmemb[0x8480] = read_sib;
    readmemb[0xa480] = read_sib;
    readmemh[0x8480] = read_sih;
    readmemh[0xa480] = read_sih;
    readmemd[0x8480] = read_sid;
    readmemd[0xa480] = read_sid;
    writemem[0x8480] = write_si;
    writemem[0xa480] = write_si;
    writememb[0x8480] = write_sib;
    writememb[0xa480] = write_sib;
    writememh[0x8480] = write_sih;
    writememh[0xa480] = write_sih;
    writememd[0x8480] = write_sid;
    writememd[0xa480] = write_sid;

    for (i=0x481; i<0x800; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    //init flashram / sram
    readmem[0x8800] = read_flashram_status;
    readmem[0xa800] = read_flashram_status;
    readmemb[0x8800] = read_flashram_statusb;
    readmemb[0xa800] = read_flashram_statusb;
    readmemh[0x8800] = read_flashram_statush;
    readmemh[0xa800] = read_flashram_statush;
    readmemd[0x8800] = read_flashram_statusd;
    readmemd[0xa800] = read_flashram_statusd;
    writemem[0x8800] = write_flashram_dummy;
    writemem[0xa800] = write_flashram_dummy;
    writememb[0x8800] = write_flashram_dummyb;
    writememb[0xa800] = write_flashram_dummyb;
    writememh[0x8800] = write_flashram_dummyh;
    writememh[0xa800] = write_flashram_dummyh;
    writememd[0x8800] = write_flashram_dummyd;
    writememd[0xa800] = write_flashram_dummyd;
    readmem[0x8801] = read_nothing;
    readmem[0xa801] = read_nothing;
    readmemb[0x8801] = read_nothingb;
    readmemb[0xa801] = read_nothingb;
    readmemh[0x8801] = read_nothingh;
    readmemh[0xa801] = read_nothingh;
    readmemd[0x8801] = read_nothingd;
    readmemd[0xa801] = read_nothingd;
    writemem[0x8801] = write_flashram_command;
    writemem[0xa801] = write_flashram_command;
    writememb[0x8801] = write_flashram_commandb;
    writememb[0xa801] = write_flashram_commandb;
    writememh[0x8801] = write_flashram_commandh;
    writememh[0xa801] = write_flashram_commandh;
    writememd[0x8801] = write_flashram_commandd;
    writememd[0xa801] = write_flashram_commandd;

    for (i=0x802; i<0x1000; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    //init rom area
    for (i=0; i<(rom_size >> 16); i++)
    {
        readmem[0x9000+i] = read_rom;
        readmem[0xb000+i] = read_rom;
        readmemb[0x9000+i] = read_romb;
        readmemb[0xb000+i] = read_romb;
        readmemh[0x9000+i] = read_romh;
        readmemh[0xb000+i] = read_romh;
        readmemd[0x9000+i] = read_romd;
        readmemd[0xb000+i] = read_romd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_rom;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }
    for (i=(rom_size >> 16); i<0xfc0; i++)
    {
        readmem[0x9000+i] = read_nothing;
        readmem[0xb000+i] = read_nothing;
        readmemb[0x9000+i] = read_nothingb;
        readmemb[0xb000+i] = read_nothingb;
        readmemh[0x9000+i] = read_nothingh;
        readmemh[0xb000+i] = read_nothingh;
        readmemd[0x9000+i] = read_nothingd;
        readmemd[0xb000+i] = read_nothingd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_nothing;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }

    /* init CIC type */
    g_cic_type = detect_cic_type(rom + 0x40);

    //init PIF_RAM
    readmem[0x9fc0] = read_pif;
    readmem[0xbfc0] = read_pif;
    readmemb[0x9fc0] = read_pifb;
    readmemb[0xbfc0] = read_pifb;
    readmemh[0x9fc0] = read_pifh;
    readmemh[0xbfc0] = read_pifh;
    readmemd[0x9fc0] = read_pifd;
    readmemd[0xbfc0] = read_pifd;
    writemem[0x9fc0] = write_pif;
    writemem[0xbfc0] = write_pif;
    writememb[0x9fc0] = write_pifb;
    writememb[0xbfc0] = write_pifb;
    writememh[0x9fc0] = write_pifh;
    writememh[0xbfc0] = write_pifh;
    writememd[0x9fc0] = write_pifd;
    writememd[0xbfc0] = write_pifd;
    for (i=0; i<(0x40/4); i++) PIF_RAM[i]=0;

    for (i=0xfc1; i<0x1000; i++)
    {
        readmem[0x9000+i] = read_nothing;
        readmem[0xb000+i] = read_nothing;
        readmemb[0x9000+i] = read_nothingb;
        readmemb[0xb000+i] = read_nothingb;
        readmemh[0x9000+i] = read_nothingh;
        readmemh[0xb000+i] = read_nothingh;
        readmemd[0x9000+i] = read_nothingd;
        readmemd[0xb000+i] = read_nothingd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_nothing;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }

    flashram_info.use_flashram = 0;
    init_flashram();

    frameBufferInfos[0].addr = 0;
    fast_memory = 1;
    firstFrameBufferSetting = 1;

    DebugMessage(M64MSG_VERBOSE, "Memory initialized");
    return 0;
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
#ifdef DBG
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        readmem[0x8000+j] = read_rdramFB_break;
                        readmemb[0x8000+j] = read_rdramFBb_break;
                        readmemh[0x8000+j] = read_rdramFBh_break;
                        readmemd[0xa000+j] = read_rdramFBd_break;
                    }
                    else
                    {
#endif
                        readmem[0x8000+j] = read_rdramFB;
                        readmemb[0x8000+j] = read_rdramFBb;
                        readmemh[0x8000+j] = read_rdramFBh;
                        readmemd[0xa000+j] = read_rdramFBd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        readmem[0xa000+j] = read_rdramFB_break;
                        readmemb[0xa000+j] = read_rdramFBb_break;
                        readmemh[0xa000+j] = read_rdramFBh_break;
                        readmemd[0x8000+j] = read_rdramFBd_break;
                    }
                    else
                    {
#endif
                        readmem[0xa000+j] = read_rdramFB;
                        readmemb[0xa000+j] = read_rdramFBb;
                        readmemh[0xa000+j] = read_rdramFBh;
                        readmemd[0x8000+j] = read_rdramFBd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        writemem[0x8000+j] = write_rdramFB_break;
                        writememb[0x8000+j] = write_rdramFBb_break;
                        writememh[0x8000+j] = write_rdramFBh_break;
                        writememd[0x8000+j] = write_rdramFBd_break;
                    }
                    else
                    {
#endif
                        writemem[0x8000+j] = write_rdramFB;
                        writememb[0x8000+j] = write_rdramFBb;
                        writememh[0x8000+j] = write_rdramFBh;
                        writememd[0x8000+j] = write_rdramFBd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        writemem[0xa000+j] = write_rdramFB_break;
                        writememb[0xa000+j] = write_rdramFBb_break;
                        writememh[0xa000+j] = write_rdramFBh_break;
                        writememd[0xa000+j] = write_rdramFBd_break;
                    }
                    else
                    {
#endif
                        writemem[0xa000+j] = write_rdramFB;
                        writememb[0xa000+j] = write_rdramFBb;
                        writememh[0xa000+j] = write_rdramFBh;
                        writememd[0xa000+j] = write_rdramFBd;
#ifdef DBG
                    }
#endif
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
#ifdef DBG
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        readmem[0x8000+j] = read_rdram_break;
                        readmemb[0x8000+j] = read_rdramb_break;
                        readmemh[0x8000+j] = read_rdramh_break;
                        readmemd[0xa000+j] = read_rdramd_break;
                    }
                    else
                    {
#endif
                        readmem[0x8000+j] = read_rdram;
                        readmemb[0x8000+j] = read_rdramb;
                        readmemh[0x8000+j] = read_rdramh;
                        readmemd[0xa000+j] = read_rdramd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        readmem[0xa000+j] = read_rdram_break;
                        readmemb[0xa000+j] = read_rdramb_break;
                        readmemh[0xa000+j] = read_rdramh_break;
                        readmemd[0x8000+j] = read_rdramd_break;
                    }
                    else
                    {
#endif
                        readmem[0xa000+j] = read_rdram;
                        readmemb[0xa000+j] = read_rdramb;
                        readmemh[0xa000+j] = read_rdramh;
                        readmemd[0x8000+j] = read_rdramd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        writemem[0x8000+j] = write_rdram_break;
                        writememb[0x8000+j] = write_rdramb_break;
                        writememh[0x8000+j] = write_rdramh_break;
                        writememd[0x8000+j] = write_rdramd_break;
                    }
                    else
                    {
#endif
                        writemem[0x8000+j] = write_rdram;
                        writememb[0x8000+j] = write_rdramb;
                        writememh[0x8000+j] = write_rdramh;
                        writememd[0x8000+j] = write_rdramd;
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        writemem[0xa000+j] = write_rdram_break;
                        writememb[0xa000+j] = write_rdramb_break;
                        writememh[0xa000+j] = write_rdramh_break;
                        writememd[0xa000+j] = write_rdramd_break;
                    }
                    else
                    {
#endif
                        writemem[0xa000+j] = write_rdram;
                        writememb[0xa000+j] = write_rdramb;
                        writememh[0xa000+j] = write_rdramh;
                        writememd[0xa000+j] = write_rdramd;
#ifdef DBG
                    }
#endif
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


void read_nothing(void)
{
    if (address == 0xa5000508) *rdword = 0xFFFFFFFF;
    else *rdword = 0;
}

void read_nothingb(void)
{
    *rdword = 0;
}

void read_nothingh(void)
{
    *rdword = 0;
}

void read_nothingd(void)
{
    *rdword = 0;
}

void write_nothing(void)
{
}

void write_nothingb(void)
{
}

void write_nothingh(void)
{
}

void write_nothingd(void)
{
}

void read_nomem(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_word_in_memory();
}

void read_nomemb(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_byte_in_memory();
}

void read_nomemh(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_hword_in_memory();
}

void read_nomemd(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_dword_in_memory();
}

void write_nomem(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_word_in_memory();
}

void write_nomemb(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_byte_in_memory();
}

void write_nomemh(void)
{
    invalidate_code(address);
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_hword_in_memory();
}

void write_nomemd(void)
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

void read_rdram(void)
{
    readw(read_rdram_ram, address, rdword);
}

void read_rdramb(void)
{
    readb(read_rdram_ram, address, rdword);
}

void read_rdramh(void)
{
    readh(read_rdram_ram, address, rdword);
}

void read_rdramd(void)
{
    readd(read_rdram_ram, address, rdword);
}

void read_rdramFB(void)
{
    pre_framebuffer_read(address);
    read_rdram();
}

void read_rdramFBb(void)
{
    pre_framebuffer_read(address);
    read_rdramb();
}

void read_rdramFBh(void)
{
    pre_framebuffer_read(address);
    read_rdramh();
}

void read_rdramFBd(void)
{
    pre_framebuffer_read(address);
    read_rdramd();
}

void write_rdram(void)
{
    writew(write_rdram_ram, address, word);
}

void write_rdramb(void)
{
    writeb(write_rdram_ram, address, cpu_byte);
}

void write_rdramh(void)
{
    writeh(write_rdram_ram, address, hword);
}

void write_rdramd(void)
{
    writed(write_rdram_ram, address, dword);
}

void write_rdramFB(void)
{
    pre_framebuffer_write(address, 4);
    write_rdram();
}

void write_rdramFBb(void)
{
    pre_framebuffer_write(address, 1);
    write_rdramb();
}

void write_rdramFBh(void)
{
    pre_framebuffer_write(address, 2);
    write_rdramh();
}

void write_rdramFBd(void)
{
    pre_framebuffer_write(address, 8);
    write_rdramd();
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

void read_rdramreg(void)
{
    readw(read_rdram_regs, address, rdword);
}

void read_rdramregb(void)
{
    readb(read_rdram_regs, address, rdword);
}

void read_rdramregh(void)
{
    readh(read_rdram_regs, address, rdword);
}

void read_rdramregd(void)
{
    readd(read_rdram_regs, address, rdword);
}

void write_rdramreg(void)
{
    writew(write_rdram_regs, address, word);
}

void write_rdramregb(void)
{
    writeb(write_rdram_regs, address, cpu_byte);
}

void write_rdramregh(void)
{
    writeh(write_rdram_regs, address, hword);
}

void write_rdramregd(void)
{
    writed(write_rdram_regs, address, dword);
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

void read_rsp_mem(void)
{
    readw(read_rspmem, address, rdword);
}

void read_rsp_memb(void)
{
    readb(read_rspmem, address, rdword);
}

void read_rsp_memh(void)
{
    readh(read_rspmem, address, rdword);
}

void read_rsp_memd(void)
{
    readd(read_rspmem, address, rdword);
}

void write_rsp_mem(void)
{
    writew(write_rspmem, address, word);
}

void write_rsp_memb(void)
{
    writeb(write_rspmem, address, cpu_byte);
}

void write_rsp_memh(void)
{
    writeh(write_rspmem, address, hword);
}

void write_rsp_memd(void)
{
    writed(write_rspmem, address, dword);
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

void read_rsp_reg(void)
{
    readw(read_rsp_regs, address, rdword);
}

void read_rsp_regb(void)
{
    readb(read_rsp_regs, address, rdword);
}

void read_rsp_regh(void)
{
    readh(read_rsp_regs, address, rdword);
}

void read_rsp_regd(void)
{
    readd(read_rsp_regs, address, rdword);
}

void write_rsp_reg(void)
{
    writew(write_rsp_regs, address, word);
}

void write_rsp_regb(void)
{
    writeb(write_rsp_regs, address, cpu_byte);
}

void write_rsp_regh(void)
{
    writeh(write_rsp_regs, address, hword);
}

void write_rsp_regd(void)
{
    writed(write_rsp_regs, address, dword);
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

void read_rsp(void)
{
    readw(read_rsp_regs2, address, rdword);
}

void read_rspb(void)
{
    readb(read_rsp_regs2, address, rdword);
}

void read_rsph(void)
{
    readh(read_rsp_regs2, address, rdword);
}

void read_rspd(void)
{
    readd(read_rsp_regs2, address, rdword);
}

void write_rsp(void)
{
    writew(write_rsp_regs2, address, word);
}

void write_rspb(void)
{
    writeb(write_rsp_regs2, address, cpu_byte);
}

void write_rsph(void)
{
    writeh(write_rsp_regs2, address, hword);
}

void write_rspd(void)
{
    writed(write_rsp_regs2, address, dword);
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

void read_dp(void)
{
    readw(read_dpc_regs, address, rdword);
}

void read_dpb(void)
{
    readb(read_dpc_regs, address, rdword);
}

void read_dph(void)
{
    readh(read_dpc_regs, address, rdword);
}

void read_dpd(void)
{
    readd(read_dpc_regs, address, rdword);
}

void write_dp(void)
{
    writew(write_dpc_regs, address, word);
}

void write_dpb(void)
{
    writeb(write_dpc_regs, address, cpu_byte);
}

void write_dph(void)
{
    writeh(write_dpc_regs, address, hword);
}

void write_dpd(void)
{
    writed(write_dpc_regs, address, dword);
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

void read_dps(void)
{
    readw(read_dps_regs, address, rdword);
}

void read_dpsb(void)
{
    readb(read_dps_regs, address, rdword);
}

void read_dpsh(void)
{
    readh(read_dps_regs, address, rdword);
}

void read_dpsd(void)
{
    readd(read_dps_regs, address, rdword);
}

void write_dps(void)
{
    writew(write_dps_regs, address, word);
}

void write_dpsb(void)
{
    writeb(write_dps_regs, address, cpu_byte);
}

void write_dpsh(void)
{
    writeh(write_dps_regs, address, hword);
}

void write_dpsd(void)
{
    writed(write_dps_regs, address, dword);
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

void read_mi(void)
{
    readw(read_mi_regs, address, rdword);
}

void read_mib(void)
{
    readb(read_mi_regs, address, rdword);
}

void read_mih(void)
{
    readh(read_mi_regs, address, rdword);
}

void read_mid(void)
{
    readd(read_mi_regs, address, rdword);
}

void write_mi(void)
{
    writew(write_mi_regs, address, word);
}

void write_mib(void)
{
    writeb(write_mi_regs, address, cpu_byte);
}

void write_mih(void)
{
    writeh(write_mi_regs, address, hword);
}

void write_mid(void)
{
    writed(write_mi_regs, address, dword);
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

void read_vi(void)
{
    readw(read_vi_regs, address, rdword);
}

void read_vib(void)
{
    readb(read_vi_regs, address, rdword);
}

void read_vih(void)
{
    readh(read_vi_regs, address, rdword);
}

void read_vid(void)
{
    readd(read_vi_regs, address, rdword);
}

void write_vi(void)
{
    writew(write_vi_regs, address, word);
}

void write_vib(void)
{
    writeb(write_vi_regs, address, cpu_byte);
}

void write_vih(void)
{
    writeh(write_vi_regs, address, hword);
}

void write_vid(void)
{
    writed(write_vi_regs, address, dword);
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

void read_ai(void)
{
    readw(read_ai_regs, address, rdword);
}

void read_aib(void)
{
    readb(read_ai_regs, address, rdword);
}

void read_aih(void)
{
    readh(read_ai_regs, address, rdword);
}

void read_aid(void)
{
    readd(read_ai_regs, address, rdword);
}

void write_ai(void)
{
    writew(write_ai_regs, address, word);
}

void write_aib(void)
{
    writeb(write_ai_regs, address, cpu_byte);
}

void write_aih(void)
{
    writeh(write_ai_regs, address, hword);
}

void write_aid(void)
{
    writed(write_ai_regs, address, dword);
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


void read_pi(void)
{
    readw(read_pi_regs, address, rdword);
}

void read_pib(void)
{
    readb(read_pi_regs, address, rdword);
}

void read_pih(void)
{
    readh(read_pi_regs, address, rdword);
}

void read_pid(void)
{
    readd(read_pi_regs, address, rdword);
}

void write_pi(void)
{
    writew(write_pi_regs, address, word);
}

void write_pib(void)
{
    writeb(write_pi_regs, address, cpu_byte);
}

void write_pih(void)
{
    writeh(write_pi_regs, address, hword);
}

void write_pid(void)
{
    writed(write_pi_regs, address, dword);
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

void read_ri(void)
{
    readw(read_ri_regs, address, rdword);
}

void read_rib(void)
{
    readb(read_ri_regs, address, rdword);
}

void read_rih(void)
{
    readh(read_ri_regs, address, rdword);
}

void read_rid(void)
{
    readd(read_ri_regs, address, rdword);
}

void write_ri(void)
{
    writew(write_ri_regs, address, word);
}

void write_rib(void)
{
    writeb(write_ri_regs, address, cpu_byte);
}

void write_rih(void)
{
    writeh(write_ri_regs, address, hword);
}

void write_rid(void)
{
    writed(write_ri_regs, address, dword);
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

void read_si(void)
{
    readw(read_si_regs, address, rdword);
}

void read_sib(void)
{
    readb(read_si_regs, address, rdword);
}

void read_sih(void)
{
    readh(read_si_regs, address, rdword);
}

void read_sid(void)
{
    readd(read_si_regs, address, rdword);
}

void write_si(void)
{
    writew(write_si_regs, address, word);
}

void write_sib(void)
{
    writeb(write_si_regs, address, cpu_byte);
}

void write_sih(void)
{
    writeh(write_si_regs, address, hword);
}

void write_sid(void)
{
    writed(write_si_regs, address, dword);
}

void read_flashram_status(void)
{
    if (flashram_info.use_flashram != -1 && *address_low == 0)
    {
        *rdword = flashram_status();
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown read in read_flashram_status()");
}

void read_flashram_statusb(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusb() not implemented");
}

void read_flashram_statush(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statush() not implemented");
}

void read_flashram_statusd(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusd() not implemented");
}

void write_flashram_dummy(void)
{
}

void write_flashram_dummyb(void)
{
}

void write_flashram_dummyh(void)
{
}

void write_flashram_dummyd(void)
{
}

void write_flashram_command(void)
{
    if (flashram_info.use_flashram != -1 && *address_low == 0)
    {
        flashram_command(word);
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown write in write_flashram_command()");
}

void write_flashram_commandb(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandb() not implemented");
}

void write_flashram_commandh(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandh() not implemented");
}

void write_flashram_commandd(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandd() not implemented");
}

static unsigned int lastwrite = 0;

void read_rom(void)
{
    if (lastwrite)
    {
        *rdword = lastwrite;
        lastwrite = 0;
    }
    else
        *rdword = *((unsigned int *)(rom + (address & 0x03FFFFFF)));
}

void read_romb(void)
{
    *rdword = *(rom + ((address^S8) & 0x03FFFFFF));
}

void read_romh(void)
{
    *rdword = *((unsigned short *)(rom + ((address^S16) & 0x03FFFFFF)));
}

void read_romd(void)
{
    *rdword = ((unsigned long long)(*((unsigned int *)(rom+(address&0x03FFFFFF))))<<32)|
              *((unsigned int *)(rom + ((address+4)&0x03FFFFFF)));
}

void write_rom(void)
{
    lastwrite = word;
}

void read_pif(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a word in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = sl(*((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)));
}

void read_pifb(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a byte in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = *(PIF_RAMb + ((address & 0x7FF) - 0x7C0));
}

void read_pifh(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a hword in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = (*(PIF_RAMb + ((address & 0x7FF) - 0x7C0)) << 8) |
              *(PIF_RAMb + (((address+1) & 0x7FF) - 0x7C0));
}

void read_pifd(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a double word in PIF in invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = ((unsigned long long)sl(*((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0))) << 32)|
              sl(*((unsigned int *)(PIF_RAMb + ((address+4) & 0x7FF) - 0x7C0)));
}

void write_pif(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a word in PIF at invalid address 0x%x", address);
        return;
    }

    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) = sl(word);
    if ((address & 0x7FF) == 0x7FC)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifb(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a byte in PIF at invalid address 0x%x", address);
        return;
    }

    *(PIF_RAMb + (address & 0x7FF) - 0x7C0) = cpu_byte;
    if ((address & 0x7FF) == 0x7FF)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifh(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a hword in PIF at invalid address 0x%x", address);
        return;
    }

    *(PIF_RAMb + (address & 0x7FF) - 0x7C0) = hword >> 8;
    *(PIF_RAMb + ((address+1) & 0x7FF) - 0x7C0) = hword & 0xFF;
    if ((address & 0x7FF) == 0x7FE)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifd(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a double word in PIF at 0x%x", address);
        return;
    }

    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) =
        sl((unsigned int)(dword >> 32));
    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) =
        sl((unsigned int)(dword & 0xFFFFFFFF));
    if ((address & 0x7FF) == 0x7F8)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
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
