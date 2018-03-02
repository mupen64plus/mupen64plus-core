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

#include "recomp.h"

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
#include "device/r4300/cp0.h"
#include "device/r4300/idec.h"
#include "device/r4300/recomp_types.h"
#include "device/r4300/recomph.h" //include for function prototypes
#include "device/r4300/tlb.h"
#include "main/main.h"
#if defined(PROFILE)
#include "main/profile.h"
#endif

#if defined(__x86_64__)
  #include "x86_64/regcache.h"
#else
  #include "x86/regcache.h"
#endif

static void *malloc_exec(size_t size);
static void free_exec(void *ptr, size_t length);

#define GENCP1_S_D(func) \
static void gencp1_##func(struct r4300_core* r4300) \
{ \
    unsigned fmt = (r4300->recomp.src >> 21) & 0x1f; \
    switch(fmt) \
    { \
    case 0x10: gen##func##_s(r4300); break; \
    case 0x11: gen##func##_d(r4300); break; \
    default: genreserved(r4300); \
    } \
}

GENCP1_S_D(abs)
GENCP1_S_D(add)
GENCP1_S_D(ceil_l)
GENCP1_S_D(ceil_w)
GENCP1_S_D(c_eq)
GENCP1_S_D(c_f)
GENCP1_S_D(c_le)
GENCP1_S_D(c_lt)
GENCP1_S_D(c_nge)
GENCP1_S_D(c_ngl)
GENCP1_S_D(c_ngle)
GENCP1_S_D(c_ngt)
GENCP1_S_D(c_ole)
GENCP1_S_D(c_olt)
GENCP1_S_D(c_seq)
GENCP1_S_D(c_sf)
GENCP1_S_D(c_ueq)
GENCP1_S_D(c_ule)
GENCP1_S_D(c_ult)
GENCP1_S_D(c_un)
GENCP1_S_D(cvt_l)
GENCP1_S_D(cvt_w)
GENCP1_S_D(div)
GENCP1_S_D(floor_l)
GENCP1_S_D(floor_w)
GENCP1_S_D(mov)
GENCP1_S_D(mul)
GENCP1_S_D(neg)
GENCP1_S_D(round_l)
GENCP1_S_D(round_w)
GENCP1_S_D(sqrt)
GENCP1_S_D(sub)
GENCP1_S_D(trunc_l)
GENCP1_S_D(trunc_w)

static void gencp1_cvt_d(struct r4300_core* r4300)
{
    unsigned fmt = (r4300->recomp.src >> 21) & 0x1f;
    switch(fmt)
    {
    case 0x10: gencvt_d_s(r4300); break;
    case 0x14: gencvt_d_w(r4300); break;
    case 0x15: gencvt_d_l(r4300); break;
    default: genreserved(r4300);
    }
}

static void gencp1_cvt_s(struct r4300_core* r4300)
{
    unsigned fmt = (r4300->recomp.src >> 21) & 0x1f;
    switch(fmt)
    {
    case 0x11: gencvt_s_d(r4300); break;
    case 0x14: gencvt_s_w(r4300); break;
    case 0x15: gencvt_s_l(r4300); break;
    default: genreserved(r4300);
    }
}

/* TODO: implement them properly */
#define genbc0f       genni
#define genbc0f_idle  genni
#define genbc0f_out   genni
#define genbc0fl      genni
#define genbc0fl_idle genni
#define genbc0fl_out  genni
#define genbc0t       genni
#define genbc0t_idle  genni
#define genbc0t_out   genni
#define genbc0tl      genni
#define genbc0tl_idle genni
#define genbc0tl_out  genni
#define genbc2f       genni
#define genbc2f_idle  genni
#define genbc2f_out   genni
#define genbc2fl      genni
#define genbc2fl_idle genni
#define genbc2fl_out  genni
#define genbc2t       genni
#define genbc2t_idle  genni
#define genbc2t_out   genni
#define genbc2tl      genni
#define genbc2tl_idle genni
#define genbc2tl_out  genni
#define genbreak      genni
#define gencfc0       genni
#define gencfc2       genni
#define genctc0       genni
#define genctc2       genni
#define gendmfc0      genni
#define gendmfc2      genni
#define gendmtc0      genni
#define gendmtc2      genni
#define genjr_idle    genni
#define genjr_out     genjr
#define genjalr_idle  genni
#define genjalr_out   genjalr
#define genldc2       genni
#define genlwc2       genni
#define genlld        genni
#define genmfc2       genni
#define genmtc2       genni
#define genscd        genni
#define gensdc2       genni
#define genswc2       genni
#define genteqi       genni
#define gentge        genni
#define gentgei       genni
#define gentgeiu      genni
#define gentgeu       genni
#define gentlt        genni
#define gentlti       genni
#define gentltiu      genni
#define gentltu       genni
#define gentne        genni
#define gentnei       genni

#define gen_bj(op) gen##op, gen##op##_idle, gen##op##_out

static void (*const recomp_funcs[R4300_OPCODES_COUNT])(struct r4300_core* r4300) =
{
    genreserved,

    genadd,
    genaddi,
    genaddiu,
    genaddu,
    genand,
    genandi,
    gen_bj(bc0f),
    gen_bj(bc0fl),
    gen_bj(bc0t),
    gen_bj(bc0tl),
    gen_bj(bc1f),
    gen_bj(bc1fl),
    gen_bj(bc1t),
    gen_bj(bc1tl),
    gen_bj(bc2f),
    gen_bj(bc2fl),
    gen_bj(bc2t),
    gen_bj(bc2tl),
    gen_bj(beq),
    gen_bj(beql),
    gen_bj(bgez),
    gen_bj(bgezal),
    gen_bj(bgezall),
    gen_bj(bgezl),
    gen_bj(bgtz),
    gen_bj(bgtzl),
    gen_bj(blez),
    gen_bj(blezl),
    gen_bj(bltz),
    gen_bj(bltzal),
    gen_bj(bltzall),
    gen_bj(bltzl),
    gen_bj(bne),
    gen_bj(bnel),
    genbreak,
    gencache,
    gencfc0,
    gencfc1,
    gencfc2,
    gencp1_abs,
    gencp1_add,
    gencp1_ceil_l,
    gencp1_ceil_w,
    gencp1_c_eq,
    gencp1_c_f,
    gencp1_c_le,
    gencp1_c_lt,
    gencp1_c_nge,
    gencp1_c_ngl,
    gencp1_c_ngle,
    gencp1_c_ngt,
    gencp1_c_ole,
    gencp1_c_olt,
    gencp1_c_seq,
    gencp1_c_sf,
    gencp1_c_ueq,
    gencp1_c_ule,
    gencp1_c_ult,
    gencp1_c_un,
    gencp1_cvt_d,
    gencp1_cvt_l,
    gencp1_cvt_s,
    gencp1_cvt_w,
    gencp1_div,
    gencp1_floor_l,
    gencp1_floor_w,
    gencp1_mov,
    gencp1_mul,
    gencp1_neg,
    gencp1_round_l,
    gencp1_round_w,
    gencp1_sqrt,
    gencp1_sub,
    gencp1_trunc_l,
    gencp1_trunc_w,
    genctc0,
    genctc1,
    genctc2,
    gendadd,
    gendaddi,
    gendaddiu,
    gendaddu,
    genddiv,
    genddivu,
    gendiv,
    gendivu,
    gendmfc0,
    gendmfc1,
    gendmfc2,
    gendmtc0,
    gendmtc1,
    gendmtc2,
    gendmult,
    gendmultu,
    gendsll,
    gendsll32,
    gendsllv,
    gendsra,
    gendsra32,
    gendsrav,
    gendsrl,
    gendsrl32,
    gendsrlv,
    gendsub,
    gendsubu,
    generet,
    gen_bj(j),
    gen_bj(jal),
    gen_bj(jalr),
    gen_bj(jr),
    genlb,
    genlbu,
    genld,
    genldc1,
    genldc2,
    genldl,
    genldr,
    genlh,
    genlhu,
    genll,
    genlld,
    genlui,
    genlw,
    genlwc1,
    genlwc2,
    genlwl,
    genlwr,
    genlwu,
    genmfc0,
    genmfc1,
    genmfc2,
    genmfhi,
    genmflo,
    genmtc0,
    genmtc1,
    genmtc2,
    genmthi,
    genmtlo,
    genmult,
    genmultu,
    gennop,
    gennor,
    genor,
    genori,
    gensb,
    gensc,
    genscd,
    gensd,
    gensdc1,
    gensdc2,
    gensdl,
    gensdr,
    gensh,
    gensll,
    gensllv,
    genslt,
    genslti,
    gensltiu,
    gensltu,
    gensra,
    gensrav,
    gensrl,
    gensrlv,
    gensub,
    gensubu,
    gensw,
    genswc1,
    genswc2,
    genswl,
    genswr,
    gensync,
    gensyscall,
    genteq,
    genteqi,
    gentge,
    gentgei,
    gentgeiu,
    gentgeu,
    gentlbp,
    gentlbr,
    gentlbwi,
    gentlbwr,
    gentlt,
    gentlti,
    gentltiu,
    gentltu,
    gentne,
    gentnei,
    genxor,
    genxori
};

/**********************************************************************
 ******************** initialize an empty block ***********************
 **********************************************************************/
void dynarec_init_block(struct r4300_core* r4300, uint32_t address)
{
    int i, length, already_exist = 1;
#if defined(PROFILE)
    timed_section_start(TIMED_SECTION_COMPILER);
#endif

    struct precomp_block** block = &r4300->cached_interp.blocks[address >> 12];

    /* allocate block */
    if (*block == NULL) {
        *block = malloc(sizeof(struct precomp_block));
        (*block)->block = NULL;
        (*block)->start = address & ~UINT32_C(0xfff);
        (*block)->end = (address & ~UINT32_C(0xfff)) + 0x1000;
        (*block)->code = NULL;
        (*block)->jumps_table = NULL;
        (*block)->riprel_table = NULL;
    }

    struct precomp_block* b = *block;

    length = get_block_length(b);

#ifdef DBG
    DebugMessage(M64MSG_INFO, "init block %" PRIX32 " - %" PRIX32, b->start, b->end);
#endif

    /* allocate block instructions */
    if (!b->block)
    {
        size_t memsize = get_block_memsize(b);
        b->block = (struct precomp_instr *) malloc_exec(memsize);
        if (!b->block) {
            DebugMessage(M64MSG_ERROR, "Memory error: couldn't allocate executable memory for dynamic recompiler. Try to use an interpreter mode.");
            return;
        }

        memset(b->block, 0, memsize);
        already_exist = 0;
    }

    if (!b->code)
    {
#if defined(PROFILE_R4300)
        r4300->recomp.max_code_length = 524288; /* allocate so much code space that we'll never have to realloc(), because this may */
        /* cause instruction locations to move, and break our profiling data                */
#else
        r4300->recomp.max_code_length = 32768;
#endif
        b->code = (unsigned char *) malloc_exec(r4300->recomp.max_code_length);
    }
    else
    {
        r4300->recomp.max_code_length = b->max_code_length;
    }

    r4300->recomp.code_length = 0;
    r4300->recomp.inst_pointer = &b->code;

    if (b->jumps_table)
    {
        free(b->jumps_table);
        b->jumps_table = NULL;
    }
    if (b->riprel_table)
    {
        free(b->riprel_table);
        b->riprel_table = NULL;
    }
    init_assembler(r4300, NULL, 0, NULL, 0);
    init_cache(r4300, b->block);

    if (!already_exist)
    {
#if defined(PROFILE_R4300)
        r4300->recomp.pfProfile = fopen("instructionaddrs.dat", "ab");
        long x86addr = (long) b->code;
        int mipsop = -2; /* -2 == NOTCOMPILED block at beginning of x86 code */
        if (fwrite(&mipsop, 1, 4, r4300->recomp.pfProfile) != 4 || // write 4-byte MIPS opcode
                fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) // write pointer to dynamically generated x86 code for this MIPS instruction
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
#endif
        for (i=0; i<length; i++)
        {
            r4300->recomp.dst = b->block + i;
            r4300->recomp.dst->addr = b->start + i*4;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
            gendebug(r4300);
#endif
            r4300->recomp.dst->ops = dynarec_notcompiled;
            gennotcompiled(r4300);
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
        r4300->recomp.code_length = b->code_length; /* leave old instructions in their place */
#else
        r4300->recomp.code_length = r4300->recomp.init_length; /* recompile everything, overwrite old recompiled instructions */
#endif
        for (i=0; i<length; i++)
        {
            r4300->recomp.dst = b->block + i;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = i * (r4300->recomp.init_length / length);
            r4300->recomp.dst->ops = r4300->cached_interp.not_compiled;
        }
    }

    free_all_registers(r4300);
    /* calling pass2 of the assembler is not necessary here because all of the code emitted by
       gennotcompiled() and gendebug() is position-independent and contains no jumps . */
    b->code_length = r4300->recomp.code_length;
    b->max_code_length = r4300->recomp.max_code_length;
    free_assembler(r4300, &b->jumps_table, &b->jumps_number, &b->riprel_table, &b->riprel_number);

    /* here we're marking the block as a valid code even if it's not compiled
     * yet as the game should have already set up the code correctly.
     */
    r4300->cached_interp.invalid_code[b->start>>12] = 0;
    if (b->end < UINT32_C(0x80000000) || b->start >= UINT32_C(0xc0000000))
    {
        uint32_t paddr = virtual_to_physical_address(r4300, b->start, 2);
        r4300->cached_interp.invalid_code[paddr>>12] = 0;
        dynarec_init_block(r4300, paddr);

        paddr += b->end - b->start - 4;
        r4300->cached_interp.invalid_code[paddr>>12] = 0;
        dynarec_init_block(r4300, paddr);

    }
    else
    {
        uint32_t alt_addr = b->start ^ UINT32_C(0x20000000);

        if (r4300->cached_interp.invalid_code[alt_addr>>12])
        {
            dynarec_init_block(r4300, alt_addr);
        }
    }
#if defined(PROFILE)
    timed_section_end(TIMED_SECTION_COMPILER);
#endif
}

void dynarec_free_block(struct precomp_block* block)
{
    size_t memsize = get_block_memsize(block);

    if (block->block) { free_exec(block->block, memsize); block->block = NULL; }
    if (block->code) { free_exec(block->code, block->max_code_length); block->code = NULL; }
    if (block->jumps_table) { free(block->jumps_table); block->jumps_table = NULL; }
    if (block->riprel_table) { free(block->riprel_table); block->riprel_table = NULL; }
}

/**********************************************************************
 ********************* recompile a block of code **********************
 **********************************************************************/
void dynarec_recompile_block(struct r4300_core* r4300, const uint32_t* iw, struct precomp_block* block, uint32_t func)
{
    int i, length, length2, finished;
    enum r4300_opcode opcode;

    /* ??? not sure why we need these 2 different tests */
    int block_start_in_tlb = ((block->start & UINT32_C(0xc0000000)) != UINT32_C(0x80000000));
    int block_not_in_tlb = (block->start >= UINT32_C(0xc0000000) || block->end < UINT32_C(0x80000000));

#if defined(PROFILE)
    timed_section_start(TIMED_SECTION_COMPILER);
#endif

    length = get_block_length(block);
    length2 = length - 2 + (length >> 2);

    /* reset xxhash */
    block->xxhash = 0;

    r4300->recomp.dst_block = block;
    r4300->recomp.code_length = block->code_length;
    r4300->recomp.max_code_length = block->max_code_length;
    r4300->recomp.inst_pointer = &block->code;
    init_assembler(r4300, block->jumps_table, block->jumps_number, block->riprel_table, block->riprel_number);
    init_cache(r4300, block->block + (func & 0xFFF) / 4);

#if defined(PROFILE_R4300)
    r4300->recomp.pfProfile = fopen("instructionaddrs.dat", "ab");
#endif

    for (i = (func & 0xFFF) / 4, finished = 0; finished != 2; ++i)
    {
        r4300->recomp.SRC = iw + i;
        r4300->recomp.src = iw[i];
        r4300->recomp.dst = block->block + i;
        r4300->recomp.dst->addr = block->start + i*4;
        r4300->recomp.dst->reg_cache_infos.need_map = 0;
        r4300->recomp.dst->local_addr = r4300->recomp.code_length;

        if (block_start_in_tlb)
        {
            uint32_t address2 = virtual_to_physical_address(r4300, r4300->recomp.dst->addr, 0);
            if (r4300->cached_interp.blocks[address2>>12]->block[(address2&UINT32_C(0xFFF))/4].ops == r4300->cached_interp.not_compiled) {
                r4300->cached_interp.blocks[address2>>12]->block[(address2&UINT32_C(0xFFF))/4].ops = r4300->cached_interp.not_compiled2;
            }
        }

#ifdef COMPARE_CORE
        gendebug(r4300);
#endif
#if defined(PROFILE_R4300)
        long x86addr = (long) (block->code + block->block[i].local_addr);

        /* write 4-byte MIPS opcode, followed by a pointer to dynamically generated x86 code for
         * this MIPS instruction. */
        if (fwrite(iw + i, 1, 4, r4300->recomp.pfProfile) != 4
        || fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) {
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
        }
#endif

        /* decode instruction */
        opcode = r4300_decode(r4300->recomp.dst, r4300, r4300_get_idec(iw[i]), iw[i], iw[i+1], block);
        recomp_funcs[opcode](r4300);

        if (r4300->recomp.delay_slot_compiled)
        {
            r4300->recomp.delay_slot_compiled--;
            free_all_registers(r4300);
        }

        /* decode ending conditions */
        if (i >= length2) { finished = 2; }
        if (i >= (length-1)
        && (block->start == UINT32_C(0xa4000000) || block_not_in_tlb)) { finished = 2; }
        if (opcode == R4300_OP_ERET || finished == 1) { finished = 2; }
        if (/*i >= length && */
                (opcode == R4300_OP_J ||
                 opcode == R4300_OP_J_OUT ||
                 opcode == R4300_OP_JR ||
                 opcode == R4300_OP_JR_OUT) &&
                !(i >= (length-1) && block_not_in_tlb)) {
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
        gendebug(r4300);
#endif
        r4300->recomp.dst->ops = dynarec_fin_block;
        genfin_block(r4300);
        ++i;
        if (i <= length2) // useful when last opcode is a jump
        {
            r4300->recomp.dst = block->block + i;
            r4300->recomp.dst->addr = block->start + i*4;
            r4300->recomp.dst->reg_cache_infos.need_map = 0;
            r4300->recomp.dst->local_addr = r4300->recomp.code_length;
#ifdef COMPARE_CORE
            gendebug(r4300);
#endif
            r4300->recomp.dst->ops = dynarec_fin_block;
            genfin_block(r4300);
            ++i;
        }
    }
    else { genlink_subblock(r4300); }

    free_all_registers(r4300);
    passe2(r4300, block->block, (func&0xFFF)/4, i, block);
    block->code_length = r4300->recomp.code_length;
    block->max_code_length = r4300->recomp.max_code_length;
    free_assembler(r4300, &block->jumps_table, &block->jumps_number, &block->riprel_table, &block->riprel_number);

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
    /* we disable next_iw == NOP check by passing 1, because we are already in delay slot */

    uint32_t iw = r4300->recomp.src;
    enum r4300_opcode opcode = r4300_decode(r4300->recomp.dst, r4300, r4300_get_idec(iw), iw, 1, r4300->recomp.dst_block);

    switch(opcode)
    {
    /* jumps/branches in delay slot are nopified */
#define CASE(op) case R4300_OP_##op:
#define JCASE(op) CASE(op) CASE(op##_IDLE) CASE(op##_OUT)
    JCASE(BC0F)
    JCASE(BC0FL)
    JCASE(BC0T)
    JCASE(BC0TL)
    JCASE(BC1F)
    JCASE(BC1FL)
    JCASE(BC1T)
    JCASE(BC1TL)
    JCASE(BC2F)
    JCASE(BC2FL)
    JCASE(BC2T)
    JCASE(BC2TL)
    JCASE(BEQ)
    JCASE(BEQL)
    JCASE(BGEZ)
    JCASE(BGEZAL)
    JCASE(BGEZALL)
    JCASE(BGEZL)
    JCASE(BGTZ)
    JCASE(BGTZL)
    JCASE(BLEZ)
    JCASE(BLEZL)
    JCASE(BLTZ)
    JCASE(BLTZAL)
    JCASE(BLTZALL)
    JCASE(BLTZL)
    JCASE(BNE)
    JCASE(BNEL)
    JCASE(J)
    JCASE(JAL)
    JCASE(JALR)
    JCASE(JR)
#undef JCASE
#undef CASE
        r4300->recomp.dst->ops = cached_interp_NOP;
        gennop(r4300);
        break;

    default:
#if defined(PROFILE_R4300)
        long x86addr = (long) ((*r4300->recomp.inst_pointer) + r4300->recomp.code_length);

        /* write 4-byte MIPS opcode, followed by a pointer to dynamically generated x86 code for
         * this MIPS instruction. */
        if (fwrite(&r4300->recomp.src, 1, 4, r4300->recomp.pfProfile) != 4
        || fwrite(&x86addr, 1, sizeof(char *), r4300->recomp.pfProfile) != sizeof(char *)) {
            DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
        }
#endif
        recomp_funcs[opcode](r4300);
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

/* Jumps to the given address. This is for the dynarec. */
void dynarec_jump_to(struct r4300_core* r4300, uint32_t address)
{
    cached_interpreter_jump_to(r4300, address);
    dyna_jump();
}

void dynarec_fin_block(void)
{
    cached_interp_FIN_BLOCK();
    dyna_jump();
}

void dynarec_notcompiled(void)
{
    cached_interp_NOTCOMPILED();
    dyna_jump();
}

void dynarec_notcompiled2(void)
{
    dynarec_notcompiled();
}

void dynarec_setup_code(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;

    /* The dynarec jumps here after we call dyna_start and it prepares
     * Here we need to prepare the initial code block and jump to it
     */
    dynarec_jump_to(r4300, UINT32_C(0xa4000040));

    /* Prevent segfault on failed dynarec_jump_to */
    if (!r4300->cached_interp.actual->block || !r4300->cached_interp.actual->code) {
        dyna_stop(r4300);
    }
}

/* Parameterless version of dynarec_jump_to to ease usage in dynarec. */
void dynarec_jump_to_recomp_address(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;

    dynarec_jump_to(r4300, r4300->recomp.jump_to_address);
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
    struct r4300_core* r4300 = &g_dev.r4300;
    uint32_t value;

    int result = r4300_read_aligned_word(
        r4300,
        r4300->recomp.address,
        &value);

    if (result)
        *r4300->recomp.rdword = value;

    return result;
}

/* Parameterless version of write_aligned_word to ease usage in dynarec. */
int dynarec_write_aligned_word(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;

    return r4300_write_aligned_word(
        r4300,
        r4300->recomp.address,
        r4300->recomp.wword,
        r4300->recomp.wmask);
}

/* Parameterless version of read_aligned_dword to ease usage in dynarec. */
int dynarec_read_aligned_dword(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;

    return r4300_read_aligned_dword(
        r4300,
        r4300->recomp.address,
        (uint64_t*)r4300->recomp.rdword);
}

/* Parameterless version of write_aligned_dword to ease usage in dynarec. */
int dynarec_write_aligned_dword(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;

    return r4300_write_aligned_dword(
        r4300,
        r4300->recomp.address,
        r4300->recomp.wdword,
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
