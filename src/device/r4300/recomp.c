/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - recomp.c                                                *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if defined(__GNUC__)
#include <unistd.h>
#ifndef __MINGW32__
#include <sys/mman.h>
#endif
#endif

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/exception.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h" //include for function prototypes
#include "device/r4300/tlb.h"
#include "main/main.h"
#if defined(PROFILE)
#include "main/profile.h"
#endif

static void *malloc_exec(size_t size);
static void free_exec(void *ptr, size_t length);

static void RSV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.RESERVED;
    r4300->recomp.recomp_func = genreserved;
}

static void RFIN_BLOCK(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.FIN_BLOCK;
    r4300->recomp.recomp_func = genfin_block;
}

static void RNOTCOMPILED(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NOTCOMPILED;
    r4300->recomp.recomp_func = gennotcompiled;
}

static void recompile_standard_i_type(struct r4300_core* r4300)
{
    r4300->recomp.dst->f.i.rs = r4300_regs(r4300) + ((r4300->recomp.src >> 21) & 0x1F);
    r4300->recomp.dst->f.i.rt = r4300_regs(r4300) + ((r4300->recomp.src >> 16) & 0x1F);
    r4300->recomp.dst->f.i.immediate = (int16_t)r4300->recomp.src;
}

static void recompile_standard_j_type(struct r4300_core* r4300)
{
    r4300->recomp.dst->f.j.inst_index = r4300->recomp.src & UINT32_C(0x3FFFFFF);
}

static void recompile_standard_r_type(struct r4300_core* r4300)
{
    r4300->recomp.dst->f.r.rs = r4300_regs(r4300) + ((r4300->recomp.src >> 21) & 0x1F);
    r4300->recomp.dst->f.r.rt = r4300_regs(r4300) + ((r4300->recomp.src >> 16) & 0x1F);
    r4300->recomp.dst->f.r.rd = r4300_regs(r4300) + ((r4300->recomp.src >> 11) & 0x1F);
    r4300->recomp.dst->f.r.sa = (r4300->recomp.src >>  6) & 0x1F;
}

static void recompile_standard_lf_type(struct r4300_core* r4300)
{
    r4300->recomp.dst->f.lf.base = (r4300->recomp.src >> 21) & 0x1F;
    r4300->recomp.dst->f.lf.ft = (r4300->recomp.src >> 16) & 0x1F;
    r4300->recomp.dst->f.lf.offset = r4300->recomp.src & 0xFFFF;
}

static void recompile_standard_cf_type(struct r4300_core* r4300)
{
    r4300->recomp.dst->f.cf.ft = (r4300->recomp.src >> 16) & 0x1F;
    r4300->recomp.dst->f.cf.fs = (r4300->recomp.src >> 11) & 0x1F;
    r4300->recomp.dst->f.cf.fd = (r4300->recomp.src >>  6) & 0x1F;
}

//-------------------------------------------------------------------------
//                                  SPECIAL                                
//-------------------------------------------------------------------------

static void RNOP(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NOP;
    r4300->recomp.recomp_func = gennop;
}

static void RSLL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLL;
    r4300->recomp.recomp_func = gensll;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSRL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SRL;
    r4300->recomp.recomp_func = gensrl;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSRA(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SRA;
    r4300->recomp.recomp_func = gensra;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSLLV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLLV;
    r4300->recomp.recomp_func = gensllv;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSRLV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SRLV;
    r4300->recomp.recomp_func = gensrlv;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSRAV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SRAV;
    r4300->recomp.recomp_func = gensrav;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RJR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.JR;
    r4300->recomp.recomp_func = genjr;
    recompile_standard_i_type(r4300);
}

static void RJALR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.JALR;
    r4300->recomp.recomp_func = genjalr;
    recompile_standard_r_type(r4300);
}

static void RSYSCALL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SYSCALL;
    r4300->recomp.recomp_func = gensyscall;
}

static void RBREAK(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RSYNC(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SYNC;
    r4300->recomp.recomp_func = gensync;
}

static void RMFHI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MFHI;
    r4300->recomp.recomp_func = genmfhi;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RMTHI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MTHI;
    r4300->recomp.recomp_func = genmthi;
    recompile_standard_r_type(r4300);
}

static void RMFLO(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MFLO;
    r4300->recomp.recomp_func = genmflo;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RMTLO(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MTLO;
    r4300->recomp.recomp_func = genmtlo;
    recompile_standard_r_type(r4300);
}

static void RDSLLV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSLLV;
    r4300->recomp.recomp_func = gendsllv;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRLV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRLV;
    r4300->recomp.recomp_func = gendsrlv;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRAV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRAV;
    r4300->recomp.recomp_func = gendsrav;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RMULT(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MULT;
    r4300->recomp.recomp_func = genmult;
    recompile_standard_r_type(r4300);
}

static void RMULTU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MULTU;
    r4300->recomp.recomp_func = genmultu;
    recompile_standard_r_type(r4300);
}

static void RDIV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DIV;
    r4300->recomp.recomp_func = gendiv;
    recompile_standard_r_type(r4300);
}

static void RDIVU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DIVU;
    r4300->recomp.recomp_func = gendivu;
    recompile_standard_r_type(r4300);
}

static void RDMULT(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DMULT;
    r4300->recomp.recomp_func = gendmult;
    recompile_standard_r_type(r4300);
}

static void RDMULTU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DMULTU;
    r4300->recomp.recomp_func = gendmultu;
    recompile_standard_r_type(r4300);
}

static void RDDIV(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DDIV;
    r4300->recomp.recomp_func = genddiv;
    recompile_standard_r_type(r4300);
}

static void RDDIVU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DDIVU;
    r4300->recomp.recomp_func = genddivu;
    recompile_standard_r_type(r4300);
}

static void RADD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADD;
    r4300->recomp.recomp_func = genadd;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RADDU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADDU;
    r4300->recomp.recomp_func = genaddu;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSUB(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SUB;
    r4300->recomp.recomp_func = gensub;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSUBU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SUBU;
    r4300->recomp.recomp_func = gensubu;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RAND(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.AND;
    r4300->recomp.recomp_func = genand;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void ROR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.OR;
    r4300->recomp.recomp_func = genor;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RXOR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.XOR;
    r4300->recomp.recomp_func = genxor;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RNOR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NOR;
    r4300->recomp.recomp_func = gennor;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSLT(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLT;
    r4300->recomp.recomp_func = genslt;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RSLTU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLTU;
    r4300->recomp.recomp_func = gensltu;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDADD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DADD;
    r4300->recomp.recomp_func = gendadd;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDADDU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DADDU;
    r4300->recomp.recomp_func = gendaddu;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSUB(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSUB;
    r4300->recomp.recomp_func = gendsub;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSUBU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSUBU;
    r4300->recomp.recomp_func = gendsubu;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RTGE(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTGEU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTLT(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTLTU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTEQ(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TEQ;
    r4300->recomp.recomp_func = genteq;
    recompile_standard_r_type(r4300);
}

static void RTNE(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RDSLL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSLL;
    r4300->recomp.recomp_func = gendsll;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRL;
    r4300->recomp.recomp_func = gendsrl;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRA(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRA;
    r4300->recomp.recomp_func = gendsra;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSLL32(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSLL32;
    r4300->recomp.recomp_func = gendsll32;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRL32(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRL32;
    r4300->recomp.recomp_func = gendsrl32;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void RDSRA32(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DSRA32;
    r4300->recomp.recomp_func = gendsra32;
    recompile_standard_r_type(r4300);
    if (r4300->recomp.dst->f.r.rd == r4300_regs(r4300)) RNOP(r4300);
}

static void (*const recomp_special[64])(struct r4300_core* r4300) =
{
    RSLL , RSV   , RSRL , RSRA , RSLLV   , RSV    , RSRLV  , RSRAV  ,
    RJR  , RJALR , RSV  , RSV  , RSYSCALL, RBREAK , RSV    , RSYNC  ,
    RMFHI, RMTHI , RMFLO, RMTLO, RDSLLV  , RSV    , RDSRLV , RDSRAV ,
    RMULT, RMULTU, RDIV , RDIVU, RDMULT  , RDMULTU, RDDIV  , RDDIVU ,
    RADD , RADDU , RSUB , RSUBU, RAND    , ROR    , RXOR   , RNOR   ,
    RSV  , RSV   , RSLT , RSLTU, RDADD   , RDADDU , RDSUB  , RDSUBU ,
    RTGE , RTGEU , RTLT , RTLTU, RTEQ    , RSV    , RTNE   , RSV    ,
    RDSLL, RSV   , RDSRL, RDSRA, RDSLL32 , RSV    , RDSRL32, RDSRA32
};

//-------------------------------------------------------------------------
//                                   REGIMM                                
//-------------------------------------------------------------------------

static void RBLTZ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZ;
    r4300->recomp.recomp_func = genbltz;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZ_IDLE;
            r4300->recomp.recomp_func = genbltz_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZ_OUT;
        r4300->recomp.recomp_func = genbltz_out;
    }
}

static void RBGEZ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZ;
    r4300->recomp.recomp_func = genbgez;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZ_IDLE;
            r4300->recomp.recomp_func = genbgez_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZ_OUT;
        r4300->recomp.recomp_func = genbgez_out;
    }
}

static void RBLTZL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZL;
    r4300->recomp.recomp_func = genbltzl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZL_IDLE;
            r4300->recomp.recomp_func = genbltzl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZL_OUT;
        r4300->recomp.recomp_func = genbltzl_out;
    }
}

static void RBGEZL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZL;
    r4300->recomp.recomp_func = genbgezl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZL_IDLE;
            r4300->recomp.recomp_func = genbgezl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZL_OUT;
        r4300->recomp.recomp_func = genbgezl_out;
    }
}

static void RTGEI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTGEIU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTLTI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTLTIU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTEQI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RTNEI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
}

static void RBLTZAL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZAL;
    r4300->recomp.recomp_func = genbltzal;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZAL_IDLE;
            r4300->recomp.recomp_func = genbltzal_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZAL_OUT;
        r4300->recomp.recomp_func = genbltzal_out;
    }
}

static void RBGEZAL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZAL;
    r4300->recomp.recomp_func = genbgezal;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZAL_IDLE;
            r4300->recomp.recomp_func = genbgezal_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZAL_OUT;
        r4300->recomp.recomp_func = genbgezal_out;
    }
}

static void RBLTZALL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZALL;
    r4300->recomp.recomp_func = genbltzall;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZALL_IDLE;
            r4300->recomp.recomp_func = genbltzall_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLTZALL_OUT;
        r4300->recomp.recomp_func = genbltzall_out;
    }
}

static void RBGEZALL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZALL;
    r4300->recomp.recomp_func = genbgezall;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZALL_IDLE;
            r4300->recomp.recomp_func = genbgezall_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGEZALL_OUT;
        r4300->recomp.recomp_func = genbgezall_out;
    }
}

static void (*const recomp_regimm[32])(struct r4300_core* r4300) =
{
    RBLTZ  , RBGEZ  , RBLTZL  , RBGEZL  , RSV  , RSV, RSV  , RSV,
    RTGEI  , RTGEIU , RTLTI   , RTLTIU  , RTEQI, RSV, RTNEI, RSV,
    RBLTZAL, RBGEZAL, RBLTZALL, RBGEZALL, RSV  , RSV, RSV  , RSV,
    RSV    , RSV    , RSV     , RSV     , RSV  , RSV, RSV  , RSV
};

//-------------------------------------------------------------------------
//                                     TLB                                 
//-------------------------------------------------------------------------

static void RTLBR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TLBR;
    r4300->recomp.recomp_func = gentlbr;
}

static void RTLBWI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TLBWI;
    r4300->recomp.recomp_func = gentlbwi;
}

static void RTLBWR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TLBWR;
    r4300->recomp.recomp_func = gentlbwr;
}

static void RTLBP(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TLBP;
    r4300->recomp.recomp_func = gentlbp;
}

static void RERET(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ERET;
    r4300->recomp.recomp_func = generet;
}

static void (*const recomp_tlb[64])(struct r4300_core* r4300) =
{
    RSV  , RTLBR, RTLBWI, RSV, RSV, RSV, RTLBWR, RSV, 
    RTLBP, RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RERET, RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV, 
    RSV  , RSV  , RSV   , RSV, RSV, RSV, RSV   , RSV
};

//-------------------------------------------------------------------------
//                                    COP0                                 
//-------------------------------------------------------------------------

static void RMFC0(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MFC0;
    r4300->recomp.recomp_func = genmfc0;
    recompile_standard_r_type(r4300);
    r4300->recomp.dst->f.r.rd = (int64_t*) (r4300_cp0_regs(&r4300->cp0) + ((r4300->recomp.src >> 11) & 0x1F));
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
    if (r4300->recomp.dst->f.r.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RMTC0(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MTC0;
    r4300->recomp.recomp_func = genmtc0;
    recompile_standard_r_type(r4300);
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
}

static void RTLB(struct r4300_core* r4300)
{
    recomp_tlb[(r4300->recomp.src & 0x3F)](r4300);
}

static void (*const recomp_cop0[32])(struct r4300_core* r4300) =
{
    RMFC0, RSV, RSV, RSV, RMTC0, RSV, RSV, RSV,
    RSV  , RSV, RSV, RSV, RSV  , RSV, RSV, RSV,
    RTLB , RSV, RSV, RSV, RSV  , RSV, RSV, RSV,
    RSV  , RSV, RSV, RSV, RSV  , RSV, RSV, RSV
};

//-------------------------------------------------------------------------
//                                     BC                                  
//-------------------------------------------------------------------------

static void RBC1F(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BC1F;
    r4300->recomp.recomp_func = genbc1f;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BC1F_IDLE;
            r4300->recomp.recomp_func = genbc1f_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BC1F_OUT;
        r4300->recomp.recomp_func = genbc1f_out;
    }
}

static void RBC1T(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BC1T;
    r4300->recomp.recomp_func = genbc1t;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BC1T_IDLE;
            r4300->recomp.recomp_func = genbc1t_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BC1T_OUT;
        r4300->recomp.recomp_func = genbc1t_out;
    }
}

static void RBC1FL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BC1FL;
    r4300->recomp.recomp_func = genbc1fl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BC1FL_IDLE;
            r4300->recomp.recomp_func = genbc1fl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BC1FL_OUT;
        r4300->recomp.recomp_func = genbc1fl_out;
    }
}

static void RBC1TL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BC1TL;
    r4300->recomp.recomp_func = genbc1tl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BC1TL_IDLE;
            r4300->recomp.recomp_func = genbc1tl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BC1TL_OUT;
        r4300->recomp.recomp_func = genbc1tl_out;
    }
}

static void (*const recomp_bc[4])(struct r4300_core* r4300) =
{
    RBC1F , RBC1T ,
    RBC1FL, RBC1TL
};

//-------------------------------------------------------------------------
//                                     S                                   
//-------------------------------------------------------------------------

static void RADD_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADD_S;
    r4300->recomp.recomp_func = genadd_s;
    recompile_standard_cf_type(r4300);
}

static void RSUB_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SUB_S;
    r4300->recomp.recomp_func = gensub_s;
    recompile_standard_cf_type(r4300);
}

static void RMUL_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MUL_S;
    r4300->recomp.recomp_func = genmul_s;
    recompile_standard_cf_type(r4300);
}

static void RDIV_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DIV_S;
    r4300->recomp.recomp_func = gendiv_s;
    recompile_standard_cf_type(r4300);
}

static void RSQRT_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SQRT_S;
    r4300->recomp.recomp_func = gensqrt_s;
    recompile_standard_cf_type(r4300);
}

static void RABS_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ABS_S;
    r4300->recomp.recomp_func = genabs_s;
    recompile_standard_cf_type(r4300);
}

static void RMOV_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MOV_S;
    r4300->recomp.recomp_func = genmov_s;
    recompile_standard_cf_type(r4300);
}

static void RNEG_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NEG_S;
    r4300->recomp.recomp_func = genneg_s;
    recompile_standard_cf_type(r4300);
}

static void RROUND_L_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ROUND_L_S;
    r4300->recomp.recomp_func = genround_l_s;
    recompile_standard_cf_type(r4300);
}

static void RTRUNC_L_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TRUNC_L_S;
    r4300->recomp.recomp_func = gentrunc_l_s;
    recompile_standard_cf_type(r4300);
}

static void RCEIL_L_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CEIL_L_S;
    r4300->recomp.recomp_func = genceil_l_s;
    recompile_standard_cf_type(r4300);
}

static void RFLOOR_L_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.FLOOR_L_S;
    r4300->recomp.recomp_func = genfloor_l_s;
    recompile_standard_cf_type(r4300);
}

static void RROUND_W_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ROUND_W_S;
    r4300->recomp.recomp_func = genround_w_s;
    recompile_standard_cf_type(r4300);
}

static void RTRUNC_W_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TRUNC_W_S;
    r4300->recomp.recomp_func = gentrunc_w_s;
    recompile_standard_cf_type(r4300);
}

static void RCEIL_W_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CEIL_W_S;
    r4300->recomp.recomp_func = genceil_w_s;
    recompile_standard_cf_type(r4300);
}

static void RFLOOR_W_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.FLOOR_W_S;
    r4300->recomp.recomp_func = genfloor_w_s;
    recompile_standard_cf_type(r4300);
}

static void RCVT_D_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_D_S;
    r4300->recomp.recomp_func = gencvt_d_s;
    recompile_standard_cf_type(r4300);
}

static void RCVT_W_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_W_S;
    r4300->recomp.recomp_func = gencvt_w_s;
    recompile_standard_cf_type(r4300);
}

static void RCVT_L_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_L_S;
    r4300->recomp.recomp_func = gencvt_l_s;
    recompile_standard_cf_type(r4300);
}

static void RC_F_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_F_S;
    r4300->recomp.recomp_func = genc_f_s;
    recompile_standard_cf_type(r4300);
}

static void RC_UN_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_UN_S;
    r4300->recomp.recomp_func = genc_un_s;
    recompile_standard_cf_type(r4300);
}

static void RC_EQ_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_EQ_S;
    r4300->recomp.recomp_func = genc_eq_s;
    recompile_standard_cf_type(r4300);
}

static void RC_UEQ_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_UEQ_S;
    r4300->recomp.recomp_func = genc_ueq_s;
    recompile_standard_cf_type(r4300);
}

static void RC_OLT_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_OLT_S;
    r4300->recomp.recomp_func = genc_olt_s;
    recompile_standard_cf_type(r4300);
}

static void RC_ULT_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_ULT_S;
    r4300->recomp.recomp_func = genc_ult_s;
    recompile_standard_cf_type(r4300);
}

static void RC_OLE_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_OLE_S;
    r4300->recomp.recomp_func = genc_ole_s;
    recompile_standard_cf_type(r4300);
}

static void RC_ULE_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_ULE_S;
    r4300->recomp.recomp_func = genc_ule_s;
    recompile_standard_cf_type(r4300);
}

static void RC_SF_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_SF_S;
    r4300->recomp.recomp_func = genc_sf_s;
    recompile_standard_cf_type(r4300);
}

static void RC_NGLE_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGLE_S;
    r4300->recomp.recomp_func = genc_ngle_s;
    recompile_standard_cf_type(r4300);
}

static void RC_SEQ_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_SEQ_S;
    r4300->recomp.recomp_func = genc_seq_s;
    recompile_standard_cf_type(r4300);
}

static void RC_NGL_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGL_S;
    r4300->recomp.recomp_func = genc_ngl_s;
    recompile_standard_cf_type(r4300);
}

static void RC_LT_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_LT_S;
    r4300->recomp.recomp_func = genc_lt_s;
    recompile_standard_cf_type(r4300);
}

static void RC_NGE_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGE_S;
    r4300->recomp.recomp_func = genc_nge_s;
    recompile_standard_cf_type(r4300);
}

static void RC_LE_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_LE_S;
    r4300->recomp.recomp_func = genc_le_s;
    recompile_standard_cf_type(r4300);
}

static void RC_NGT_S(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGT_S;
    r4300->recomp.recomp_func = genc_ngt_s;
    recompile_standard_cf_type(r4300);
}

static void (*const recomp_s[64])(struct r4300_core* r4300) =
{
    RADD_S    , RSUB_S    , RMUL_S   , RDIV_S    , RSQRT_S   , RABS_S    , RMOV_S   , RNEG_S    , 
    RROUND_L_S, RTRUNC_L_S, RCEIL_L_S, RFLOOR_L_S, RROUND_W_S, RTRUNC_W_S, RCEIL_W_S, RFLOOR_W_S, 
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
    RSV       , RCVT_D_S  , RSV      , RSV       , RCVT_W_S  , RCVT_L_S  , RSV      , RSV       , 
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       , 
    RC_F_S    , RC_UN_S   , RC_EQ_S  , RC_UEQ_S  , RC_OLT_S  , RC_ULT_S  , RC_OLE_S , RC_ULE_S  , 
    RC_SF_S   , RC_NGLE_S , RC_SEQ_S , RC_NGL_S  , RC_LT_S   , RC_NGE_S  , RC_LE_S  , RC_NGT_S
};

//-------------------------------------------------------------------------
//                                     D                                   
//-------------------------------------------------------------------------

static void RADD_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADD_D;
    r4300->recomp.recomp_func = genadd_d;
    recompile_standard_cf_type(r4300);
}

static void RSUB_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SUB_D;
    r4300->recomp.recomp_func = gensub_d;
    recompile_standard_cf_type(r4300);
}

static void RMUL_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MUL_D;
    r4300->recomp.recomp_func = genmul_d;
    recompile_standard_cf_type(r4300);
}

static void RDIV_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DIV_D;
    r4300->recomp.recomp_func = gendiv_d;
    recompile_standard_cf_type(r4300);
}

static void RSQRT_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SQRT_D;
    r4300->recomp.recomp_func = gensqrt_d;
    recompile_standard_cf_type(r4300);
}

static void RABS_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ABS_D;
    r4300->recomp.recomp_func = genabs_d;
    recompile_standard_cf_type(r4300);
}

static void RMOV_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MOV_D;
    r4300->recomp.recomp_func = genmov_d;
    recompile_standard_cf_type(r4300);
}

static void RNEG_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NEG_D;
    r4300->recomp.recomp_func = genneg_d;
    recompile_standard_cf_type(r4300);
}

static void RROUND_L_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ROUND_L_D;
    r4300->recomp.recomp_func = genround_l_d;
    recompile_standard_cf_type(r4300);
}

static void RTRUNC_L_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TRUNC_L_D;
    r4300->recomp.recomp_func = gentrunc_l_d;
    recompile_standard_cf_type(r4300);
}

static void RCEIL_L_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CEIL_L_D;
    r4300->recomp.recomp_func = genceil_l_d;
    recompile_standard_cf_type(r4300);
}

static void RFLOOR_L_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.FLOOR_L_D;
    r4300->recomp.recomp_func = genfloor_l_d;
    recompile_standard_cf_type(r4300);
}

static void RROUND_W_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ROUND_W_D;
    r4300->recomp.recomp_func = genround_w_d;
    recompile_standard_cf_type(r4300);
}

static void RTRUNC_W_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.TRUNC_W_D;
    r4300->recomp.recomp_func = gentrunc_w_d;
    recompile_standard_cf_type(r4300);
}

static void RCEIL_W_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CEIL_W_D;
    r4300->recomp.recomp_func = genceil_w_d;
    recompile_standard_cf_type(r4300);
}

static void RFLOOR_W_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.FLOOR_W_D;
    r4300->recomp.recomp_func = genfloor_w_d;
    recompile_standard_cf_type(r4300);
}

static void RCVT_S_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_S_D;
    r4300->recomp.recomp_func = gencvt_s_d;
    recompile_standard_cf_type(r4300);
}

static void RCVT_W_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_W_D;
    r4300->recomp.recomp_func = gencvt_w_d;
    recompile_standard_cf_type(r4300);
}

static void RCVT_L_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_L_D;
    r4300->recomp.recomp_func = gencvt_l_d;
    recompile_standard_cf_type(r4300);
}

static void RC_F_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_F_D;
    r4300->recomp.recomp_func = genc_f_d;
    recompile_standard_cf_type(r4300);
}

static void RC_UN_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_UN_D;
    r4300->recomp.recomp_func = genc_un_d;
    recompile_standard_cf_type(r4300);
}

static void RC_EQ_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_EQ_D;
    r4300->recomp.recomp_func = genc_eq_d;
    recompile_standard_cf_type(r4300);
}

static void RC_UEQ_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_UEQ_D;
    r4300->recomp.recomp_func = genc_ueq_d;
    recompile_standard_cf_type(r4300);
}

static void RC_OLT_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_OLT_D;
    r4300->recomp.recomp_func = genc_olt_d;
    recompile_standard_cf_type(r4300);
}

static void RC_ULT_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_ULT_D;
    r4300->recomp.recomp_func = genc_ult_d;
    recompile_standard_cf_type(r4300);
}

static void RC_OLE_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_OLE_D;
    r4300->recomp.recomp_func = genc_ole_d;
    recompile_standard_cf_type(r4300);
}

static void RC_ULE_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_ULE_D;
    r4300->recomp.recomp_func = genc_ule_d;
    recompile_standard_cf_type(r4300);
}

static void RC_SF_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_SF_D;
    r4300->recomp.recomp_func = genc_sf_d;
    recompile_standard_cf_type(r4300);
}

static void RC_NGLE_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGLE_D;
    r4300->recomp.recomp_func = genc_ngle_d;
    recompile_standard_cf_type(r4300);
}

static void RC_SEQ_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_SEQ_D;
    r4300->recomp.recomp_func = genc_seq_d;
    recompile_standard_cf_type(r4300);
}

static void RC_NGL_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGL_D;
    r4300->recomp.recomp_func = genc_ngl_d;
    recompile_standard_cf_type(r4300);
}

static void RC_LT_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_LT_D;
    r4300->recomp.recomp_func = genc_lt_d;
    recompile_standard_cf_type(r4300);
}

static void RC_NGE_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGE_D;
    r4300->recomp.recomp_func = genc_nge_d;
    recompile_standard_cf_type(r4300);
}

static void RC_LE_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_LE_D;
    r4300->recomp.recomp_func = genc_le_d;
    recompile_standard_cf_type(r4300);
}

static void RC_NGT_D(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.C_NGT_D;
    r4300->recomp.recomp_func = genc_ngt_d;
    recompile_standard_cf_type(r4300);
}

static void (*const recomp_d[64])(struct r4300_core* r4300) =
{
    RADD_D    , RSUB_D    , RMUL_D   , RDIV_D    , RSQRT_D   , RABS_D    , RMOV_D   , RNEG_D    ,
    RROUND_L_D, RTRUNC_L_D, RCEIL_L_D, RFLOOR_L_D, RROUND_W_D, RTRUNC_W_D, RCEIL_W_D, RFLOOR_W_D,
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
    RCVT_S_D  , RSV       , RSV      , RSV       , RCVT_W_D  , RCVT_L_D  , RSV      , RSV       ,
    RSV       , RSV       , RSV      , RSV       , RSV       , RSV       , RSV      , RSV       ,
    RC_F_D    , RC_UN_D   , RC_EQ_D  , RC_UEQ_D  , RC_OLT_D  , RC_ULT_D  , RC_OLE_D , RC_ULE_D  ,
    RC_SF_D   , RC_NGLE_D , RC_SEQ_D , RC_NGL_D  , RC_LT_D   , RC_NGE_D  , RC_LE_D  , RC_NGT_D
};

//-------------------------------------------------------------------------
//                                     W                                   
//-------------------------------------------------------------------------

static void RCVT_S_W(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_S_W;
    r4300->recomp.recomp_func = gencvt_s_w;
    recompile_standard_cf_type(r4300);
}

static void RCVT_D_W(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_D_W;
    r4300->recomp.recomp_func = gencvt_d_w;
    recompile_standard_cf_type(r4300);
}

static void (*const recomp_w[64])(struct r4300_core* r4300) =
{
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RCVT_S_W, RCVT_D_W, RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV
};

//-------------------------------------------------------------------------
//                                     L                                   
//-------------------------------------------------------------------------

static void RCVT_S_L(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_S_L;
    r4300->recomp.recomp_func = gencvt_s_l;
    recompile_standard_cf_type(r4300);
}

static void RCVT_D_L(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CVT_D_L;
    r4300->recomp.recomp_func = gencvt_d_l;
    recompile_standard_cf_type(r4300);
}

static void (*const recomp_l[64])(struct r4300_core* r4300) =
{
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV,
    RCVT_S_L, RCVT_D_L, RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
    RSV     , RSV     , RSV, RSV, RSV, RSV, RSV, RSV, 
};

//-------------------------------------------------------------------------
//                                    COP1                                 
//-------------------------------------------------------------------------

static void RMFC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MFC1;
    r4300->recomp.recomp_func = genmfc1;
    recompile_standard_r_type(r4300);
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
    if (r4300->recomp.dst->f.r.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RDMFC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DMFC1;
    r4300->recomp.recomp_func = gendmfc1;
    recompile_standard_r_type(r4300);
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
    if (r4300->recomp.dst->f.r.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RCFC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CFC1;
    r4300->recomp.recomp_func = gencfc1;
    recompile_standard_r_type(r4300);
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
    if (r4300->recomp.dst->f.r.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RMTC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.MTC1;
    recompile_standard_r_type(r4300);
    r4300->recomp.recomp_func = genmtc1;
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
}

static void RDMTC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DMTC1;
    recompile_standard_r_type(r4300);
    r4300->recomp.recomp_func = gendmtc1;
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
}

static void RCTC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.CTC1;
    recompile_standard_r_type(r4300);
    r4300->recomp.recomp_func = genctc1;
    r4300->recomp.dst->f.r.nrd = (r4300->recomp.src >> 11) & 0x1F;
}

static void RBC(struct r4300_core* r4300)
{
    recomp_bc[((r4300->recomp.src >> 16) & 3)](r4300);
}

static void RS(struct r4300_core* r4300)
{
    recomp_s[(r4300->recomp.src & 0x3F)](r4300);
}

static void RD(struct r4300_core* r4300)
{
    recomp_d[(r4300->recomp.src & 0x3F)](r4300);
}

static void RW(struct r4300_core* r4300)
{
    recomp_w[(r4300->recomp.src & 0x3F)](r4300);
}

static void RL(struct r4300_core* r4300)
{
    recomp_l[(r4300->recomp.src & 0x3F)](r4300);
}

static void (*const recomp_cop1[32])(struct r4300_core* r4300) =
{
    RMFC1, RDMFC1, RCFC1, RSV, RMTC1, RDMTC1, RCTC1, RSV,
    RBC  , RSV   , RSV  , RSV, RSV  , RSV   , RSV  , RSV,
    RS   , RD    , RSV  , RSV, RW   , RL    , RSV  , RSV,
    RSV  , RSV   , RSV  , RSV, RSV  , RSV   , RSV  , RSV
};

//-------------------------------------------------------------------------
//                                   R4300                                 
//-------------------------------------------------------------------------

static void RSPECIAL(struct r4300_core* r4300)
{
    recomp_special[(r4300->recomp.src & 0x3F)](r4300);
}

static void RREGIMM(struct r4300_core* r4300)
{
    recomp_regimm[((r4300->recomp.src >> 16) & 0x1F)](r4300);
}

static void RJ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.J;
    r4300->recomp.recomp_func = genj;
    recompile_standard_j_type(r4300);
    target = (r4300->recomp.dst->f.j.inst_index<<2) | (r4300->recomp.dst->addr & UINT32_C(0xF0000000));
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.J_IDLE;
            r4300->recomp.recomp_func = genj_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.J_OUT;
        r4300->recomp.recomp_func = genj_out;
    }
}

static void RJAL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.JAL;
    r4300->recomp.recomp_func = genjal;
    recompile_standard_j_type(r4300);
    target = (r4300->recomp.dst->f.j.inst_index<<2) | (r4300->recomp.dst->addr & UINT32_C(0xF0000000));
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.JAL_IDLE;
            r4300->recomp.recomp_func = genjal_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.JAL_OUT;
        r4300->recomp.recomp_func = genjal_out;
    }
}

static void RBEQ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BEQ;
    r4300->recomp.recomp_func = genbeq;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BEQ_IDLE;
            r4300->recomp.recomp_func = genbeq_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BEQ_OUT;
        r4300->recomp.recomp_func = genbeq_out;
    }
}

static void RBNE(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BNE;
    r4300->recomp.recomp_func = genbne;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BNE_IDLE;
            r4300->recomp.recomp_func = genbne_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BNE_OUT;
        r4300->recomp.recomp_func = genbne_out;
    }
}

static void RBLEZ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZ;
    r4300->recomp.recomp_func = genblez;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZ_IDLE;
            r4300->recomp.recomp_func = genblez_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZ_OUT;
        r4300->recomp.recomp_func = genblez_out;
    }
}

static void RBGTZ(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZ;
    r4300->recomp.recomp_func = genbgtz;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZ_IDLE;
            r4300->recomp.recomp_func = genbgtz_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZ_OUT;
        r4300->recomp.recomp_func = genbgtz_out;
    }
}

static void RADDI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADDI;
    r4300->recomp.recomp_func = genaddi;
    recompile_standard_i_type(r4300);
    if(r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RADDIU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ADDIU;
    r4300->recomp.recomp_func = genaddiu;
    recompile_standard_i_type(r4300);
    if(r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RSLTI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLTI;
    r4300->recomp.recomp_func = genslti;
    recompile_standard_i_type(r4300);
    if(r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RSLTIU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SLTIU;
    r4300->recomp.recomp_func = gensltiu;
    recompile_standard_i_type(r4300);
    if(r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RANDI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ANDI;
    r4300->recomp.recomp_func = genandi;
    recompile_standard_i_type(r4300);
    if(r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RORI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.ORI;
    r4300->recomp.recomp_func = genori;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RXORI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.XORI;
    r4300->recomp.recomp_func = genxori;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLUI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LUI;
    r4300->recomp.recomp_func = genlui;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RCOP0(struct r4300_core* r4300)
{
    recomp_cop0[((r4300->recomp.src >> 21) & 0x1F)](r4300);
}

static void RCOP1(struct r4300_core* r4300)
{
    recomp_cop1[((r4300->recomp.src >> 21) & 0x1F)](r4300);
}

static void RBEQL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BEQL;
    r4300->recomp.recomp_func = genbeql;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BEQL_IDLE;
            r4300->recomp.recomp_func = genbeql_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BEQL_OUT;
        r4300->recomp.recomp_func = genbeql_out;
    }
}

static void RBNEL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BNEL;
    r4300->recomp.recomp_func = genbnel;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BNEL_IDLE;
            r4300->recomp.recomp_func = genbnel_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BNEL_OUT;
        r4300->recomp.recomp_func = genbnel_out;
    }
}

static void RBLEZL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZL;
    r4300->recomp.recomp_func = genblezl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZL_IDLE;
            r4300->recomp.recomp_func = genblezl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BLEZL_OUT;
        r4300->recomp.recomp_func = genblezl_out;
    }
}

static void RBGTZL(struct r4300_core* r4300)
{
    uint32_t target;
    r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZL;
    r4300->recomp.recomp_func = genbgtzl;
    recompile_standard_i_type(r4300);
    target = r4300->recomp.dst->addr + r4300->recomp.dst->f.i.immediate*4 + 4;
    if (target == r4300->recomp.dst->addr)
    {
        if (r4300->recomp.check_nop)
        {
            r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZL_IDLE;
            r4300->recomp.recomp_func = genbgtzl_idle;
        }
    }
    else if (target < r4300->recomp.dst_block->start || target >= r4300->recomp.dst_block->end || r4300->recomp.dst->addr == (r4300->recomp.dst_block->end-4))
    {
        r4300->recomp.dst->ops = r4300->current_instruction_table.BGTZL_OUT;
        r4300->recomp.recomp_func = genbgtzl_out;
    }
}

static void RDADDI(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DADDI;
    r4300->recomp.recomp_func = gendaddi;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RDADDIU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.DADDIU;
    r4300->recomp.recomp_func = gendaddiu;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLDL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LDL;
    r4300->recomp.recomp_func = genldl;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLDR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LDR;
    r4300->recomp.recomp_func = genldr;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLB(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LB;
    r4300->recomp.recomp_func = genlb;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLH(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LH;
    r4300->recomp.recomp_func = genlh;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLWL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LWL;
    r4300->recomp.recomp_func = genlwl;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLW(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LW;
    r4300->recomp.recomp_func = genlw;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLBU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LBU;
    r4300->recomp.recomp_func = genlbu;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLHU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LHU;
    r4300->recomp.recomp_func = genlhu;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLWR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LWR;
    r4300->recomp.recomp_func = genlwr;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLWU(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LWU;
    r4300->recomp.recomp_func = genlwu;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RSB(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SB;
    r4300->recomp.recomp_func = gensb;
    recompile_standard_i_type(r4300);
}

static void RSH(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SH;
    r4300->recomp.recomp_func = gensh;
    recompile_standard_i_type(r4300);
}

static void RSWL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SWL;
    r4300->recomp.recomp_func = genswl;
    recompile_standard_i_type(r4300);
}

static void RSW(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SW;
    r4300->recomp.recomp_func = gensw;
    recompile_standard_i_type(r4300);
}

static void RSDL(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SDL;
    r4300->recomp.recomp_func = gensdl;
    recompile_standard_i_type(r4300);
}

static void RSDR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SDR;
    r4300->recomp.recomp_func = gensdr;
    recompile_standard_i_type(r4300);
}

static void RSWR(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SWR;
    r4300->recomp.recomp_func = genswr;
    recompile_standard_i_type(r4300);
}

static void RCACHE(struct r4300_core* r4300)
{
    r4300->recomp.recomp_func = gencache;
    r4300->recomp.dst->ops = r4300->current_instruction_table.CACHE;
}

static void RLL(struct r4300_core* r4300)
{
    r4300->recomp.recomp_func = genll;
    r4300->recomp.dst->ops = r4300->current_instruction_table.LL;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RLWC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LWC1;
    r4300->recomp.recomp_func = genlwc1;
    recompile_standard_lf_type(r4300);
}

static void RLLD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
    recompile_standard_i_type(r4300);
}

static void RLDC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LDC1;
    r4300->recomp.recomp_func = genldc1;
    recompile_standard_lf_type(r4300);
}

static void RLD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.LD;
    r4300->recomp.recomp_func = genld;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RSC(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SC;
    r4300->recomp.recomp_func = gensc;
    recompile_standard_i_type(r4300);
    if (r4300->recomp.dst->f.i.rt == r4300_regs(r4300)) RNOP(r4300);
}

static void RSWC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SWC1;
    r4300->recomp.recomp_func = genswc1;
    recompile_standard_lf_type(r4300);
}

static void RSCD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.NI;
    r4300->recomp.recomp_func = genni;
    recompile_standard_i_type(r4300);
}

static void RSDC1(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SDC1;
    r4300->recomp.recomp_func = gensdc1;
    recompile_standard_lf_type(r4300);
}

static void RSD(struct r4300_core* r4300)
{
    r4300->recomp.dst->ops = r4300->current_instruction_table.SD;
    r4300->recomp.recomp_func = gensd;
    recompile_standard_i_type(r4300);
}

static void (*const recomp_ops[64])(struct r4300_core* r4300) =
{
    RSPECIAL, RREGIMM, RJ   , RJAL  , RBEQ , RBNE , RBLEZ , RBGTZ ,
    RADDI   , RADDIU , RSLTI, RSLTIU, RANDI, RORI , RXORI , RLUI  ,
    RCOP0   , RCOP1  , RSV  , RSV   , RBEQL, RBNEL, RBLEZL, RBGTZL,
    RDADDI  , RDADDIU, RLDL , RLDR  , RSV  , RSV  , RSV   , RSV   ,
    RLB     , RLH    , RLWL , RLW   , RLBU , RLHU , RLWR  , RLWU  ,
    RSB     , RSH    , RSWL , RSW   , RSDL , RSDR , RSWR  , RCACHE,
    RLL     , RLWC1  , RSV  , RSV   , RLLD , RLDC1, RSV   , RLD   ,
    RSC     , RSWC1  , RSV  , RSV   , RSCD , RSDC1, RSV   , RSD
};

static int get_block_length(const struct precomp_block *block)
{
    return (block->end-block->start)/4;
}

static size_t get_block_memsize(const struct precomp_block *block)
{
    int length = get_block_length(block);
    return ((length+1)+(length>>2)) * sizeof(struct precomp_instr);
}

/**********************************************************************
 ******************** initialize an empty block ***********************
 **********************************************************************/
void init_block(struct r4300_core* r4300, struct precomp_block* block)
{
    int i, length, already_exist = 1;
#if defined(PROFILE)
    timed_section_start(TIMED_SECTION_COMPILER);
#endif
#ifdef DBG
    DebugMessage(M64MSG_INFO, "init block %" PRIX32 " - %" PRIX32, block->start, block->end);
#endif

    length = get_block_length(block);

    if (!block->block)
    {
        size_t memsize = get_block_memsize(block);
        if (r4300->emumode == EMUMODE_DYNAREC) {
            block->block = (struct precomp_instr *) malloc_exec(memsize);
            if (!block->block) {
                DebugMessage(M64MSG_ERROR, "Memory error: couldn't allocate executable memory for dynamic recompiler. Try to use an interpreter mode.");
                return;
            }
        }
        else {
            block->block = (struct precomp_instr *) malloc(memsize);
            if (!block->block) {
                DebugMessage(M64MSG_ERROR, "Memory error: couldn't allocate memory for cached interpreter.");
                return;
            }
        }

        memset(block->block, 0, memsize);
        already_exist = 0;
    }

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        if (!block->code)
        {
#if defined(PROFILE_R4300)
            r4300->recomp.max_code_length = 524288; /* allocate so much code space that we'll never have to realloc(), because this may */
            /* cause instruction locations to move, and break our profiling data                */
#else
            r4300->recomp.max_code_length = 32768;
#endif
            block->code = (unsigned char *) malloc_exec(r4300->recomp.max_code_length);
        }
        else
        {
            r4300->recomp.max_code_length = block->max_code_length;
        }
        r4300->recomp.code_length = 0;
        r4300->recomp.inst_pointer = &block->code;

        if (block->jumps_table)
        {
            free(block->jumps_table);
            block->jumps_table = NULL;
        }
        if (block->riprel_table)
        {
            free(block->riprel_table);
            block->riprel_table = NULL;
        }
        init_assembler(NULL, 0, NULL, 0);
        init_cache(block->block);
    }

    if (!already_exist)
    {
#if defined(PROFILE_R4300)
        r4300->recomp.pfProfile = fopen("instructionaddrs.dat", "ab");
        long x86addr = (long) block->code;
        int mipsop = -2; /* -2 == NOTCOMPILED block at beginning of x86 code */
        if (fwrite(&mipsop, 1, 4, r4300->recomp.pfProfile) != 4 || // write 4-byte MIPS opcode
                fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) // write pointer to dynamically generated x86 code for this MIPS instruction
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
#endif

        for (i=0; i<length; i++)
        {
            r4300->recomp.dst = block->block + i;
            r4300->recomp.dst->addr = block->start + i*4;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
            if (r4300->emumode == EMUMODE_DYNAREC) gendebug(r4300);
#endif
            RNOTCOMPILED(r4300);
            if (r4300->emumode == EMUMODE_DYNAREC) r4300->recomp.recomp_func(r4300);
        }
#if defined(PROFILE_R4300)
        fclose(r4300->recomp.pfProfile);
        r4300->recomp.pfProfile = NULL;
#endif
        r4300->recomp.init_length = r4300->recomp.code_length;
    }
    else
    {
#if defined(PROFILE_R4300)
        r4300->recomp.code_length = block->code_length; /* leave old instructions in their place */
#else
        r4300->recomp.code_length = r4300->recomp.init_length; /* recompile everything, overwrite old recompiled instructions */
#endif
        for (i=0; i<length; i++)
        {
            r4300->recomp.dst = block->block + i;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = i * (r4300->recomp.init_length / length);
            r4300->recomp.dst->ops = r4300->current_instruction_table.NOTCOMPILED;
        }
    }

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        free_all_registers();
        /* calling pass2 of the assembler is not necessary here because all of the code emitted by
           gennotcompiled() and gendebug() is position-independent and contains no jumps . */
        block->code_length = r4300->recomp.code_length;
        block->max_code_length = r4300->recomp.max_code_length;
        free_assembler(&block->jumps_table, &block->jumps_number, &block->riprel_table, &block->riprel_number);
    }

    /* here we're marking the block as a valid code even if it's not compiled
     * yet as the game should have already set up the code correctly.
     */
    r4300->cached_interp.invalid_code[block->start>>12] = 0;
    if (block->end < UINT32_C(0x80000000) || block->start >= UINT32_C(0xc0000000))
    {
        uint32_t paddr = virtual_to_physical_address(r4300, block->start, 2);
        r4300->cached_interp.invalid_code[paddr>>12] = 0;
        if (!r4300->cached_interp.blocks[paddr>>12])
        {
            r4300->cached_interp.blocks[paddr>>12] = (struct precomp_block *) malloc(sizeof(struct precomp_block));
            r4300->cached_interp.blocks[paddr>>12]->code = NULL;
            r4300->cached_interp.blocks[paddr>>12]->block = NULL;
            r4300->cached_interp.blocks[paddr>>12]->jumps_table = NULL;
            r4300->cached_interp.blocks[paddr>>12]->riprel_table = NULL;
            r4300->cached_interp.blocks[paddr>>12]->start = paddr & ~UINT32_C(0xFFF);
            r4300->cached_interp.blocks[paddr>>12]->end = (paddr & ~UINT32_C(0xFFF)) + UINT32_C(0x1000);
        }
        init_block(r4300, r4300->cached_interp.blocks[paddr>>12]);

        paddr += block->end - block->start - 4;
        r4300->cached_interp.invalid_code[paddr>>12] = 0;
        if (!r4300->cached_interp.blocks[paddr>>12])
        {
            r4300->cached_interp.blocks[paddr>>12] = (struct precomp_block *) malloc(sizeof(struct precomp_block));
            r4300->cached_interp.blocks[paddr>>12]->code = NULL;
            r4300->cached_interp.blocks[paddr>>12]->block = NULL;
            r4300->cached_interp.blocks[paddr>>12]->jumps_table = NULL;
            r4300->cached_interp.blocks[paddr>>12]->riprel_table = NULL;
            r4300->cached_interp.blocks[paddr>>12]->start = paddr & ~UINT32_C(0xFFF);
            r4300->cached_interp.blocks[paddr>>12]->end = (paddr & ~UINT32_C(0xFFF)) + UINT32_C(0x1000);
        }
        init_block(r4300, r4300->cached_interp.blocks[paddr>>12]);
    }
    else
    {
        uint32_t alt_addr = block->start ^ UINT32_C(0x20000000);

        if (r4300->cached_interp.invalid_code[alt_addr>>12])
        {
            if (!r4300->cached_interp.blocks[alt_addr>>12])
            {
                r4300->cached_interp.blocks[alt_addr>>12] = (struct precomp_block *) malloc(sizeof(struct precomp_block));
                r4300->cached_interp.blocks[alt_addr>>12]->code = NULL;
                r4300->cached_interp.blocks[alt_addr>>12]->block = NULL;
                r4300->cached_interp.blocks[alt_addr>>12]->jumps_table = NULL;
                r4300->cached_interp.blocks[alt_addr>>12]->riprel_table = NULL;
                r4300->cached_interp.blocks[alt_addr>>12]->start = alt_addr & ~UINT32_C(0xFFF);
                r4300->cached_interp.blocks[alt_addr>>12]->end = (alt_addr & ~UINT32_C(0xFFF)) + UINT32_C(0x1000);
            }
            init_block(r4300, r4300->cached_interp.blocks[alt_addr>>12]);
        }
    }
#if defined(PROFILE)
    timed_section_end(TIMED_SECTION_COMPILER);
#endif
}

void free_block(struct r4300_core* r4300, struct precomp_block* block)
{
    size_t memsize = get_block_memsize(block);

    if (block->block) {
        if (r4300->emumode == EMUMODE_DYNAREC) {
            free_exec(block->block, memsize);
        }
        else {
            free(block->block);
        }
        block->block = NULL;
    }
    if (block->code) { free_exec(block->code, block->max_code_length); block->code = NULL; }
    if (block->jumps_table) { free(block->jumps_table); block->jumps_table = NULL; }
    if (block->riprel_table) { free(block->riprel_table); block->riprel_table = NULL; }
}

/**********************************************************************
 ********************* recompile a block of code **********************
 **********************************************************************/
void recompile_block(struct r4300_core* r4300, const uint32_t* source, struct precomp_block* block, uint32_t func)
{
    int i;
    int length, finished = 0;
#if defined(PROFILE)
    timed_section_start(TIMED_SECTION_COMPILER);
#endif
    length = (block->end-block->start)/4;
    r4300->recomp.dst_block = block;

    block->xxhash = 0;

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        r4300->recomp.code_length = block->code_length;
        r4300->recomp.max_code_length = block->max_code_length;
        r4300->recomp.inst_pointer = &block->code;
        init_assembler(block->jumps_table, block->jumps_number, block->riprel_table, block->riprel_number);
        init_cache(block->block + (func & 0xFFF) / 4);
    }

#if defined(PROFILE_R4300)
    r4300->recomp.pfProfile = fopen("instructionaddrs.dat", "ab");
#endif

    for (i = (func & 0xFFF) / 4; finished != 2; i++)
    {
        if (block->start < UINT32_C(0x80000000) || UINT32_C(block->start >= 0xc0000000))
        {
            uint32_t address2 = virtual_to_physical_address(r4300, block->start + i*4, 0);
            if (r4300->cached_interp.blocks[address2>>12]->block[(address2&UINT32_C(0xFFF))/4].ops == r4300->current_instruction_table.NOTCOMPILED) {
                r4300->cached_interp.blocks[address2>>12]->block[(address2&UINT32_C(0xFFF))/4].ops = r4300->current_instruction_table.NOTCOMPILED2;
            }
        }

        r4300->recomp.SRC = source + i;
        r4300->recomp.src = source[i];
        r4300->recomp.check_nop = source[i+1] == 0;
        r4300->recomp.dst = block->block + i;
        r4300->recomp.dst->addr = block->start + i*4;
        r4300->recomp.dst->reg_cache_infos.need_map = 0;
        r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
        if (r4300->emumode == EMUMODE_DYNAREC) { gendebug(r4300); }
#endif
#if defined(PROFILE_R4300)
        long x86addr = (long) (block->code + block->block[i].local_addr);

        /* write 4-byte MIPS opcode, followed by a pointer to dynamically generated x86 code for
         * this MIPS instruction. */
        if (fwrite(source + i, 1, 4, r4300->recomp.pfProfile) != 4
        || fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) {
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
        }
#endif
        r4300->recomp.recomp_func = NULL;
        recomp_ops[((r4300->recomp.src >> 26) & 0x3F)](r4300);
        if (r4300->emumode == EMUMODE_DYNAREC) { r4300->recomp.recomp_func(r4300); }
        r4300->recomp.dst = block->block + i;

        /*if ((r4300->recomp.dst+1)->ops != NOTCOMPILED && !r4300->recomp.delay_slot_compiled &&
          i < length)
          {
          if (r4300->emumode == EMUMODE_DYNAREC) genlink_subblock(r4300);
          finished = 2;
          }*/
        if (r4300->recomp.delay_slot_compiled)
        {
            r4300->recomp.delay_slot_compiled--;
            free_all_registers();
        }

        if (i >= length-2+(length>>2)) { finished = 2; }
        if (i >= (length-1) && (block->start == UINT32_C(0xa4000000) ||
                    block->start >= UINT32_C(0xc0000000) ||
                    block->end   <  UINT32_C(0x80000000))) { finished = 2; }
        if (r4300->recomp.dst->ops == r4300->current_instruction_table.ERET || finished == 1) { finished = 2; }
        if (/*i >= length && */
                (r4300->recomp.dst->ops == r4300->current_instruction_table.J ||
                 r4300->recomp.dst->ops == r4300->current_instruction_table.J_OUT ||
                 r4300->recomp.dst->ops == r4300->current_instruction_table.JR) &&
                !(i >= (length-1) && (block->start >= UINT32_C(0xc0000000) ||
                        block->end   <  UINT32_C(0x80000000)))) {
            finished = 1;
        }
    }

#if defined(PROFILE_R4300)
    long x86addr = (long) (block->code + r4300->recomp.code_length);
    int mipsop = -3; /* -3 == block-postfix */
    /* write 4-byte MIPS opcode, followed by a pointer to dynamically generated x86 code for
     * this MIPS instruction. */
    if (fwrite(&mipsop, 1, 4, r4300->recomp.pfProfile) != 4
    || fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) {
        DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
    }
#endif

    if (i >= length)
    {
        r4300->recomp.dst = block->block + i;
        r4300->recomp.dst->addr = block->start + i*4;
        r4300->recomp.dst->reg_cache_infos.need_map = 0;
        r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
        if (r4300->emumode == EMUMODE_DYNAREC) { gendebug(r4300); }
#endif
        RFIN_BLOCK(r4300);
        if (r4300->emumode == EMUMODE_DYNAREC) { r4300->recomp.recomp_func(r4300); }
        i++;
        if (i < length-1+(length>>2)) // useful when last opcode is a jump
        {
            r4300->recomp.dst = block->block + i;
            r4300->recomp.dst->addr = block->start + i*4;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
            if (r4300->emumode == EMUMODE_DYNAREC) { gendebug(r4300); }
#endif
            RFIN_BLOCK(r4300);
            if (r4300->emumode == EMUMODE_DYNAREC) { r4300->recomp.recomp_func(r4300); }
            i++;
        }
    }
    else if (r4300->emumode == EMUMODE_DYNAREC) { genlink_subblock(r4300); }

    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        free_all_registers();
        passe2(block->block, (func&0xFFF)/4, i, block);
        block->code_length = r4300->recomp.code_length;
        block->max_code_length = r4300->recomp.max_code_length;
        free_assembler(&block->jumps_table, &block->jumps_number, &block->riprel_table, &block->riprel_number);
    }
#ifdef DBG
    DebugMessage(M64MSG_INFO, "block recompiled (%" PRIX32 "-%" PRIX32 ")", func, block->start+i*4);
#endif
#if defined(PROFILE_R4300)
    fclose(r4300->recomp.pfProfile);
    r4300->recomp.pfProfile = NULL;
#endif
#if defined(PROFILE)
    timed_section_end(TIMED_SECTION_COMPILER);
#endif
}

static int is_jump(const struct r4300_core* r4300)
{
    return
        (r4300->recomp.dst->ops == r4300->current_instruction_table.J ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.J_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.J_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.JAL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.JAL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.JAL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQ ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQ_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQ_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNE_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNE_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZ ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZ_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZ_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZ ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZ_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZ_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BEQL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNEL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNEL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BNEL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLEZL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGTZL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.JR ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.JALR ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZ ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZ_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZ_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZ ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZ_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZ_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZAL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZAL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZAL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZAL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZAL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZAL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZALL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZALL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BLTZALL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZALL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZALL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BGEZALL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1F ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1F_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1F_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1T ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1T_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1T_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1FL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1FL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1FL_IDLE ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1TL ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1TL_OUT ||
         r4300->recomp.dst->ops == r4300->current_instruction_table.BC1TL_IDLE);
}

/**********************************************************************
 ************ recompile only one opcode (use for delay slot) **********
 **********************************************************************/
void recompile_opcode(struct r4300_core* r4300)
{
    r4300->recomp.SRC++;
    r4300->recomp.src = *r4300->recomp.SRC;
    r4300->recomp.dst++;
    r4300->recomp.dst->addr = (r4300->recomp.dst-1)->addr + 4;
    r4300->recomp.dst->reg_cache_infos.need_map = 0;

    recomp_ops[((r4300->recomp.src >> 26) & 0x3F)](r4300);
    if (!is_jump(r4300))
    {
#if defined(PROFILE_R4300)
        long x86addr = (long) ((*r4300->recomp.inst_pointer) + r4300->recomp.code_length);

        /* write 4-byte MIPS opcode, followed by a pointer to dynamically generated x86 code for
         * this MIPS instruction. */
        if (fwrite(&r4300->recomp.src, 1, 4, r4300->recomp.pfProfile) != 4
        || fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) {
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
        }
#endif
        r4300->recomp.recomp_func = NULL;
        recomp_ops[((r4300->recomp.src >> 26) & 0x3F)](r4300);
        if (r4300->emumode == EMUMODE_DYNAREC) { r4300->recomp.recomp_func(r4300); }
    }
    else
    {
        RNOP(r4300);
        if (r4300->emumode == EMUMODE_DYNAREC) { r4300->recomp.recomp_func(r4300); }
    }
    r4300->recomp.delay_slot_compiled = 2;
}

#if defined(PROFILE_R4300)
void profile_write_end_of_code_blocks(struct r4300_core* r4300)
{
    size_t i;

    r4300->recomp.pfProfile = fopen("instructionaddrs.dat", "ab");

    for (i = 0; i < 0x100000; ++i) {
        if (r4300->cached_interp.invalid_code[i] == 0 && r4300->cached_interp.blocks[i] != NULL && r4300->cached_interp.blocks[i]->code != NULL && r4300->cached_interp.blocks[i]->block != NULL)
        {
            unsigned char *x86addr;
            int mipsop;
            // store final code length for this block
            mipsop = -1; /* -1 == end of x86 code block */
            x86addr = r4300->cached_interp.blocks[i]->code + r4300->cached_interp.blocks[i]->code_length;
            if (fwrite(&mipsop, 1, 4, r4300->recomp.pfProfile) != 4 ||
                    fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *))
                DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
        }
    }

    fclose(r4300->recomp.pfProfile);
    r4300->recomp.pfProfile = NULL;
}
#endif


/* Parameterless version of cached_interpreter_dynarec_jump_to to ease usage in dynarec. */
void dynarec_jump_to_address(void)
{
    cached_interpreter_dynarec_jump_to(&g_dev.r4300, g_dev.r4300.recomp.jump_to_address);
}

/* Parameterless version of exception_general to ease usage in dynarec. */
void dynarec_exception_general(void)
{
    exception_general(&g_dev.r4300);
}

/* Parameterless version of check_cop1_unusable to ease usage in dynarec. */
int dynarec_check_cop1_unusable(void)
{
    return check_cop1_unusable(&g_dev.r4300);
}


/* Parameterless version of cp0_update_count to ease usage in dynarec. */
void dynarec_cp0_update_count(void)
{
    cp0_update_count(&g_dev.r4300);
}

/* Parameterless version of gen_interrupt to ease usage in dynarec. */
void dynarec_gen_interrupt(void)
{
    gen_interrupt(&g_dev.r4300);
}

/* Parameterless version of read_aligned_word to ease usage in dynarec. */
int dynarec_read_aligned_word(void)
{
    uint32_t value;

    int result = r4300_read_aligned_word(
        &g_dev.r4300,
        *r4300_address(&g_dev.r4300),
        &value);

    if (result)
        *g_dev.r4300.rdword = value;

    return result;
}

/* Parameterless version of write_aligned_word to ease usage in dynarec. */
int dynarec_write_aligned_word(void)
{
    return r4300_write_aligned_word(
        &g_dev.r4300,
        *r4300_address(&g_dev.r4300),
        *r4300_wword(&g_dev.r4300),
        *r4300_wmask(&g_dev.r4300));
}

/* Parameterless version of read_aligned_dword to ease usage in dynarec. */
int dynarec_read_aligned_dword(void)
{
    return r4300_read_aligned_dword(
        &g_dev.r4300,
        *r4300_address(&g_dev.r4300),
        (uint64_t*)g_dev.r4300.rdword);
}

/* Parameterless version of write_aligned_dword to ease usage in dynarec. */
int dynarec_write_aligned_dword(void)
{
    return r4300_write_aligned_dword(
        &g_dev.r4300,
        *r4300_address(&g_dev.r4300),
        *r4300_wdword(&g_dev.r4300),
        ~UINT64_C(0)); /* NOTE: in dynarec, we only need all-one masks */
}



/**********************************************************************
 ************** allocate memory with executable bit set ***************
 **********************************************************************/
static void *malloc_exec(size_t size)
{
#if defined(WIN32)
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#elif defined(__GNUC__)

#ifndef  MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

    void *block = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED)
    { DebugMessage(M64MSG_ERROR, "Memory error: couldn't allocate %zi byte block of aligned RWX memory.", size); return NULL; }

    return block;
#else
    return malloc(size);
#endif
}

/**********************************************************************
 ************* reallocate memory with executable bit set **************
 **********************************************************************/
void *realloc_exec(void *ptr, size_t oldsize, size_t newsize)
{
    void* block = malloc_exec(newsize);
    if (block != NULL)
    {
        size_t copysize;
        copysize = (oldsize < newsize)
            ? oldsize
            : newsize;
        memcpy(block, ptr, copysize);
    }
    free_exec(ptr, oldsize);
    return block;
}

/**********************************************************************
 **************** frees memory with executable bit set ****************
 **********************************************************************/
static void free_exec(void *ptr, size_t length)
{
#if defined(WIN32)
    VirtualFree(ptr, 0, MEM_RELEASE);
#elif defined(__GNUC__)
    munmap(ptr, length);
#else
    free(ptr);
#endif
}
