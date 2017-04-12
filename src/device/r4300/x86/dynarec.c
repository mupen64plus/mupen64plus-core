/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dynarec.c                                               *
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

#include "assemble.h"
#include "interpret.h"
#include "regcache.h"

#include "api/callbacks.h"
#include "api/debugger.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/cp0.h"
#include "device/r4300/cp1.h"
#include "device/r4300/exception.h"
#include "device/r4300/interrupt.h"
#include "device/r4300/macros.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "main/main.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


/* These are constants with addresses so that FLDCW can read them.
 * They are declared 'extern' so that other files can do the same. */
const uint16_t trunc_mode = 0xf3f;
const uint16_t round_mode = 0x33f;
const uint16_t ceil_mode  = 0xb3f;
const uint16_t floor_mode = 0x73f;

static const unsigned int precomp_instr_size = sizeof(struct precomp_instr);

/* Dynarec control functions */

void dyna_jump()
{
    if (*r4300_stop(&g_dev.r4300) == 1)
    {
        dyna_stop();
        return;
    }

    if ((*r4300_pc_struct(&g_dev.r4300))->reg_cache_infos.need_map)
    {
        *g_dev.r4300.return_address = (unsigned long) ((*r4300_pc_struct(&g_dev.r4300))->reg_cache_infos.jump_wrapper);
    }
    else
    {
        *g_dev.r4300.return_address = (unsigned long) (g_dev.r4300.cached_interp.actual->code + (*r4300_pc_struct(&g_dev.r4300))->local_addr);
    }
}

void dyna_stop()
{
    if (g_dev.r4300.save_eip == 0)
    {
        DebugMessage(M64MSG_WARNING, "instruction pointer is 0 at dyna_stop()");
    }
    else
    {
        *g_dev.r4300.return_address = (unsigned long) g_dev.r4300.save_eip;
    }
}


/* M64P Pseudo instructions */

static void gencheck_cop1_unusable(void)
{
    free_all_registers();
    simplify_access();
    test_m32_imm32((unsigned int*)&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_STATUS_REG], CP0_STATUS_CU1);
    jne_rj(0);

    jump_start_rel8();

    gencallinterp((unsigned int)dynarec_check_cop1_unusable, 0);

    jump_end_rel8();
}

static void gencp0_update_count(unsigned int addr)
{
#if !defined(COMPARE_CORE) && !defined(DBG)
    mov_reg32_imm32(EAX, addr);
    sub_reg32_m32(EAX, (unsigned int*)(&g_dev.r4300.cp0.last_addr));
    shr_reg32_imm8(EAX, 2);
    mov_reg32_m32(EDX, &g_dev.r4300.cp0.count_per_op);
    mul_reg32(EDX);
    add_m32_reg32((unsigned int*)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]), EAX);
#else
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_cp0_update_count);
    call_reg32(EAX);
#endif
}

static void gencheck_interrupt(unsigned int instr_structure)
{
    mov_eax_memoffs32(r4300_cp0_next_interrupt(&g_dev.r4300.cp0));
    cmp_reg32_m32(EAX, &r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]);
    ja_rj(17);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), instr_structure); // 10
    mov_reg32_imm32(EAX, (unsigned int)gen_interrupt); // 5
    call_reg32(EAX); // 2
}

static void gencheck_interrupt_out(unsigned int addr)
{
    mov_eax_memoffs32(r4300_cp0_next_interrupt(&g_dev.r4300.cp0));
    cmp_reg32_m32(EAX, &r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]);
    ja_rj(27);
    mov_m32_imm32((unsigned int*)(&g_dev.r4300.fake_instr.addr), addr);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(&g_dev.r4300.fake_instr));
    mov_reg32_imm32(EAX, (unsigned int)gen_interrupt);
    call_reg32(EAX);
}

static void gencheck_interrupt_reg(void) // addr is in EAX
{
    mov_reg32_m32(EBX, r4300_cp0_next_interrupt(&g_dev.r4300.cp0));
    cmp_reg32_m32(EBX, &r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]);
    ja_rj(22);
    mov_memoffs32_eax((unsigned int*)(&g_dev.r4300.fake_instr.addr)); // 5
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(&g_dev.r4300.fake_instr)); // 10
    mov_reg32_imm32(EAX, (unsigned int)gen_interrupt); // 5
    call_reg32(EAX); // 2
}

#ifdef COMPARE_CORE
extern unsigned int op; /* api/debugger.c */

void gendebug(void)
{
    free_all_registers();

    mov_m32_reg32((unsigned int*)&g_dev.r4300.eax, EAX);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.ebx, EBX);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.ecx, ECX);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.edx, EDX);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.esp, ESP);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.ebp, EBP);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.esi, ESI);
    mov_m32_reg32((unsigned int*)&g_dev.r4300.edi, EDI);

    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst));
    mov_m32_imm32((unsigned int*)(&op), (unsigned int)(g_dev.r4300.recomp.src));
    mov_reg32_imm32(EAX, (unsigned int) CoreCompareCallback);
    call_reg32(EAX);

    mov_reg32_m32(EAX, (unsigned int*)&g_dev.r4300.eax);
    mov_reg32_m32(EBX, (unsigned int*)&g_dev.r4300.ebx);
    mov_reg32_m32(ECX, (unsigned int*)&g_dev.r4300.ecx);
    mov_reg32_m32(EDX, (unsigned int*)&g_dev.r4300.edx);
    mov_reg32_m32(ESP, (unsigned int*)&g_dev.r4300.esp);
    mov_reg32_m32(EBP, (unsigned int*)&g_dev.r4300.ebp);
    mov_reg32_m32(ESI, (unsigned int*)&g_dev.r4300.esi);
    mov_reg32_m32(EDI, (unsigned int*)&g_dev.r4300.edi);
}
#endif

void genni(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.NI, 0);
}

void gennotcompiled(void)
{
    free_all_registers();
    simplify_access();

    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst));
    mov_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED);
    call_reg32(EAX);
}

void genlink_subblock(void)
{
    free_all_registers();
    jmp(g_dev.r4300.recomp.dst->addr+4);
}

void genfin_block(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.FIN_BLOCK, 0);
}

void gencallinterp(uintptr_t addr, int jump)
{
    free_all_registers();
    simplify_access();

    if (jump) {
        mov_m32_imm32((unsigned int*)(&g_dev.r4300.dyna_interp), 1);
    }

    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst));
    mov_reg32_imm32(EAX, addr);
    call_reg32(EAX);

    if (jump)
    {
        mov_m32_imm32((unsigned int*)(&g_dev.r4300.dyna_interp), 0);
        mov_reg32_imm32(EAX, (unsigned int)dyna_jump);
        call_reg32(EAX);
    }
}

void gendelayslot(void)
{
    mov_m32_imm32(&g_dev.r4300.delay_slot, 1);
    recompile_opcode(&g_dev.r4300);

    free_all_registers();
    gencp0_update_count(g_dev.r4300.recomp.dst->addr+4);

    mov_m32_imm32(&g_dev.r4300.delay_slot, 0);
}

/* Reserved */

void genreserved(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.RESERVED, 0);
}

/* Load instructions */

void genlb(void)
{
#ifdef INTERPRET_LB
    gencallinterp((unsigned int)cached_interpreter_table.LB, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemb);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramb);
    }
    je_rj(47);

    mov_m32_imm32((unsigned int *)&(*r4300_pc_struct(&g_dev.r4300)), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemb); // 7
    call_reg32(EBX); // 2
    movsx_reg32_m8(EAX, (unsigned char *)g_dev.r4300.recomp.dst->f.i.rt); // 7
    jmp_imm_short(16); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 3); // 3
    movsx_reg32_8preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 7

    set_register_state(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genlbu(void)
{
#ifdef INTERPRET_LBU
    gencallinterp((unsigned int)cached_interpreter_table.LBU, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemb);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramb);
    }
    je_rj(46);

    mov_m32_imm32((unsigned int *)&(*r4300_pc_struct(&g_dev.r4300)), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemb); // 7
    call_reg32(EBX); // 2
    mov_reg32_m32(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt); // 6
    jmp_imm_short(15); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 3); // 3
    mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6

    and_eax_imm32(0xFF);

    set_register_state(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genlh(void)
{
#ifdef INTERPRET_LH
    gencallinterp((unsigned int)cached_interpreter_table.LH, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemh);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramh);
    }
    je_rj(47);

    mov_m32_imm32((unsigned int *)&(*r4300_pc_struct(&g_dev.r4300)), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemh); // 7
    call_reg32(EBX); // 2
    movsx_reg32_m16(EAX, (unsigned short *)g_dev.r4300.recomp.dst->f.i.rt); // 7
    jmp_imm_short(16); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 2); // 3
    movsx_reg32_16preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 7

    set_register_state(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genlhu(void)
{
#ifdef INTERPRET_LHU
    gencallinterp((unsigned int)cached_interpreter_table.LHU, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemh);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramh);
    }
    je_rj(46);

    mov_m32_imm32((unsigned int *)&(*r4300_pc_struct(&g_dev.r4300)), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemh); // 7
    call_reg32(EBX); // 2
    mov_reg32_m32(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt); // 6
    jmp_imm_short(15); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 2); // 3
    mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6

    and_eax_imm32(0xFFFF);

    set_register_state(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genll(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.LL, 0);
}

void genlw(void)
{
#ifdef INTERPRET_LW
    gencallinterp((unsigned int)cached_interpreter_table.LW, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmem);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdram);
    }
    je_rj(45);

    mov_m32_imm32((unsigned int *)&(*r4300_pc_struct(&g_dev.r4300)), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmem); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(g_dev.r4300.recomp.dst->f.i.rt)); // 5
    jmp_imm_short(12); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6

    set_register_state(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genlwu(void)
{
#ifdef INTERPRET_LWU
    gencallinterp((unsigned int)cached_interpreter_table.LWU, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmem);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdram);
    }
    je_rj(45);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmem); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(g_dev.r4300.recomp.dst->f.i.rt)); // 5
    jmp_imm_short(12); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6

    xor_reg32_reg32(EBX, EBX);

    set_64_register_state(EAX, EBX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genlwl(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.LWL, 0);
}

void genlwr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.LWR, 0);
}

void genld(void)
{
#ifdef INTERPRET_LD
    gencallinterp((unsigned int)cached_interpreter_table.LD, 0);
#else
    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemd);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramd);
    }
    je_rj(51);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_imm32((unsigned int *)(&g_dev.r4300.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemd); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(g_dev.r4300.recomp.dst->f.i.rt)); // 5
    mov_reg32_m32(ECX, (unsigned int *)(g_dev.r4300.recomp.dst->f.i.rt)+1); // 6
    jmp_imm_short(18); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg32pimm32(EAX, EBX, ((unsigned int)g_dev.ri.rdram.dram)+4); // 6
    mov_reg32_preg32pimm32(ECX, EBX, ((unsigned int)g_dev.ri.rdram.dram)); // 6

    set_64_register_state(EAX, ECX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1);
#endif
}

void genldl(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.LDL, 0);
}

void genldr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.LDR, 0);
}

/* Store instructions */

void gensb(void)
{
#ifdef INTERPRET_SB
    gencallinterp((unsigned int)cached_interpreter_table.SB, 0);
#else
    free_all_registers();
    simplify_access();
    mov_reg8_m8(CL, (unsigned char *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writememb);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdramb);
    }
    je_rj(41);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m8_reg8((unsigned char *)(r4300_wbyte(&g_dev.r4300)), CL); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememb); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(17); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 3); // 3
    mov_preg32pimm32_reg8(EBX, (unsigned int)g_dev.ri.rdram.dram, CL); // 6

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void gensh(void)
{
#ifdef INTERPRET_SH
    gencallinterp((unsigned int)cached_interpreter_table.SH, 0);
#else
    free_all_registers();
    simplify_access();
    mov_reg16_m16(CX, (unsigned short *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writememh);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdramh);
    }
    je_rj(42);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m16_reg16((unsigned short *)(r4300_whword(&g_dev.r4300)), CX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememh); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(18); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 2); // 3
    mov_preg32pimm32_reg16(EBX, (unsigned int)g_dev.ri.rdram.dram, CX); // 7

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void gensc(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.SC, 0);
}

void gensw(void)
{
#ifdef INTERPRET_SW
    gencallinterp((unsigned int)cached_interpreter_table.SW, 0);
#else
    free_all_registers();
    simplify_access();
    mov_reg32_m32(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writemem);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdram);
    }
    je_rj(41);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wword(&g_dev.r4300)), ECX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writemem); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(14); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg32pimm32_reg32(EBX, (unsigned int)g_dev.ri.rdram.dram, ECX); // 6

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void genswl(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.SWL, 0);
}

void genswr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.SWR, 0);
}

void gensd(void)
{
#ifdef INTERPRET_SD
    gencallinterp((unsigned int)cached_interpreter_table.SD, 0);
#else
    free_all_registers();
    simplify_access();

    mov_reg32_m32(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_reg32_m32(EDX, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt)+1);
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writememd);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdramd);
    }
    je_rj(47);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wdword(&g_dev.r4300)), ECX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wdword(&g_dev.r4300))+1, EDX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememd); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(20); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg32pimm32_reg32(EBX, ((unsigned int)g_dev.ri.rdram.dram)+4, ECX); // 6
    mov_preg32pimm32_reg32(EBX, ((unsigned int)g_dev.ri.rdram.dram)+0, EDX); // 6

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void gensdl(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.SDL, 0);
}

void gensdr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.SDR, 0);
}

/* Computational instructions */

void genadd(void)
{
#ifdef INTERPRET_ADD
    gencallinterp((unsigned int)cached_interpreter_table.ADD, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt != rd && rs != rd)
    {
        mov_reg32_reg32(rd, rs);
        add_reg32_reg32(rd, rt);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs);
        add_reg32_reg32(temp, rt);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void genaddu(void)
{
#ifdef INTERPRET_ADDU
    gencallinterp((unsigned int)cached_interpreter_table.ADDU, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt != rd && rs != rd)
    {
        mov_reg32_reg32(rd, rs);
        add_reg32_reg32(rd, rt);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs);
        add_reg32_reg32(temp, rt);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void genaddi(void)
{
#ifdef INTERPRET_ADDI
    gencallinterp((unsigned int)cached_interpreter_table.ADDI, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genaddiu(void)
{
#ifdef INTERPRET_ADDIU
    gencallinterp((unsigned int)cached_interpreter_table.ADDIU, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gendadd(void)
{
#ifdef INTERPRET_DADD
    gencallinterp((unsigned int)cached_interpreter_table.DADD, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        add_reg32_reg32(rd1, rt1);
        adc_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        add_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        adc_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void gendaddu(void)
{
#ifdef INTERPRET_DADDU
    gencallinterp((unsigned int)cached_interpreter_table.DADDU, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        add_reg32_reg32(rd1, rt1);
        adc_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        add_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        adc_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void gendaddi(void)
{
#ifdef INTERPRET_DADDI
    gencallinterp((unsigned int)cached_interpreter_table.DADDI, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    int rt2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt1, rs1);
    mov_reg32_reg32(rt2, rs2);
    add_reg32_imm32(rt1, g_dev.r4300.recomp.dst->f.i.immediate);
    adc_reg32_imm32(rt2, (int)g_dev.r4300.recomp.dst->f.i.immediate>>31);
#endif
}

void gendaddiu(void)
{
#ifdef INTERPRET_DADDIU
    gencallinterp((unsigned int)cached_interpreter_table.DADDIU, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    int rt2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt1, rs1);
    mov_reg32_reg32(rt2, rs2);
    add_reg32_imm32(rt1, g_dev.r4300.recomp.dst->f.i.immediate);
    adc_reg32_imm32(rt2, (int)g_dev.r4300.recomp.dst->f.i.immediate>>31);
#endif
}

void gensub(void)
{
#ifdef INTERPRET_SUB
    gencallinterp((unsigned int)cached_interpreter_table.SUB, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt != rd && rs != rd)
    {
        mov_reg32_reg32(rd, rs);
        sub_reg32_reg32(rd, rt);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs);
        sub_reg32_reg32(temp, rt);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void gensubu(void)
{
#ifdef INTERPRET_SUBU
    gencallinterp((unsigned int)cached_interpreter_table.SUBU, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt != rd && rs != rd)
    {
        mov_reg32_reg32(rd, rs);
        sub_reg32_reg32(rd, rt);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs);
        sub_reg32_reg32(temp, rt);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void gendsub(void)
{
#ifdef INTERPRET_DSUB
    gencallinterp((unsigned int)cached_interpreter_table.DSUB, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        sub_reg32_reg32(rd1, rt1);
        sbb_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        sub_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        sbb_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void gendsubu(void)
{
#ifdef INTERPRET_DSUBU
    gencallinterp((unsigned int)cached_interpreter_table.DSUBU, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        sub_reg32_reg32(rd1, rt1);
        sbb_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        sub_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        sbb_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void genslt(void)
{
#ifdef INTERPRET_SLT
    gencallinterp((unsigned int)cached_interpreter_table.SLT, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    cmp_reg32_reg32(rs2, rt2);
    jl_rj(13);
    jne_rj(4); // 2
    cmp_reg32_reg32(rs1, rt1); // 2
    jl_rj(7); // 2
    mov_reg32_imm32(rd, 0); // 5
    jmp_imm_short(5); // 2
    mov_reg32_imm32(rd, 1); // 5
#endif
}

void gensltu(void)
{
#ifdef INTERPRET_SLTU
    gencallinterp((unsigned int)cached_interpreter_table.SLTU, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    cmp_reg32_reg32(rs2, rt2);
    jb_rj(13);
    jne_rj(4); // 2
    cmp_reg32_reg32(rs1, rt1); // 2
    jb_rj(7); // 2
    mov_reg32_imm32(rd, 0); // 5
    jmp_imm_short(5); // 2
    mov_reg32_imm32(rd, 1); // 5
#endif
}

void genslti(void)
{
#ifdef INTERPRET_SLTI
    gencallinterp((unsigned int)cached_interpreter_table.SLTI, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    long long imm = (long long)g_dev.r4300.recomp.dst->f.i.immediate;

    cmp_reg32_imm32(rs2, (unsigned int)(imm >> 32));
    jl_rj(17);
    jne_rj(8); // 2
    cmp_reg32_imm32(rs1, (unsigned int)imm); // 6
    jl_rj(7); // 2
    mov_reg32_imm32(rt, 0); // 5
    jmp_imm_short(5); // 2
    mov_reg32_imm32(rt, 1); // 5
#endif
}

void gensltiu(void)
{
#ifdef INTERPRET_SLTIU
    gencallinterp((unsigned int)cached_interpreter_table.SLTIU, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    long long imm = (long long)g_dev.r4300.recomp.dst->f.i.immediate;

    cmp_reg32_imm32(rs2, (unsigned int)(imm >> 32));
    jb_rj(17);
    jne_rj(8); // 2
    cmp_reg32_imm32(rs1, (unsigned int)imm); // 6
    jb_rj(7); // 2
    mov_reg32_imm32(rt, 0); // 5
    jmp_imm_short(5); // 2
    mov_reg32_imm32(rt, 1); // 5
#endif
}

void genand(void)
{
#ifdef INTERPRET_AND
    gencallinterp((unsigned int)cached_interpreter_table.AND, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        and_reg32_reg32(rd1, rt1);
        and_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        and_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        and_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void genandi(void)
{
#ifdef INTERPRET_ANDI
    gencallinterp((unsigned int)cached_interpreter_table.ANDI, 0);
#else
    int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    and_reg32_imm32(rt, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genor(void)
{
#ifdef INTERPRET_OR
    gencallinterp((unsigned int)cached_interpreter_table.OR, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        or_reg32_reg32(rd1, rt1);
        or_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        or_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        or_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void genori(void)
{
#ifdef INTERPRET_ORI
    gencallinterp((unsigned int)cached_interpreter_table.ORI, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    int rt2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt1, rs1);
    mov_reg32_reg32(rt2, rs2);
    or_reg32_imm32(rt1, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genxor(void)
{
#ifdef INTERPRET_XOR
    gencallinterp((unsigned int)cached_interpreter_table.XOR, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        xor_reg32_reg32(rd1, rt1);
        xor_reg32_reg32(rd2, rt2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        xor_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        xor_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
    }
#endif
}

void genxori(void)
{
#ifdef INTERPRET_XORI
    gencallinterp((unsigned int)cached_interpreter_table.XORI, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    int rt2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt1, rs1);
    mov_reg32_reg32(rt2, rs2);
    xor_reg32_imm32(rt1, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gennor(void)
{
#ifdef INTERPRET_NOR
    gencallinterp((unsigned int)cached_interpreter_table.NOR, 0);
#else
    int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rt1 != rd1 && rs1 != rd1)
    {
        mov_reg32_reg32(rd1, rs1);
        mov_reg32_reg32(rd2, rs2);
        or_reg32_reg32(rd1, rt1);
        or_reg32_reg32(rd2, rt2);
        not_reg32(rd1);
        not_reg32(rd2);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rs1);
        or_reg32_reg32(temp, rt1);
        mov_reg32_reg32(rd1, temp);
        mov_reg32_reg32(temp, rs2);
        or_reg32_reg32(temp, rt2);
        mov_reg32_reg32(rd2, temp);
        not_reg32(rd1);
        not_reg32(rd2);
    }
#endif
}

void genlui(void)
{
#ifdef INTERPRET_LUI
    gencallinterp((unsigned int)cached_interpreter_table.LUI, 0);
#else
    int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_imm32(rt, (unsigned int)g_dev.r4300.recomp.dst->f.i.immediate << 16);
#endif
}

/* Shift instructions */

void gennop(void)
{
}

void gensll(void)
{
#ifdef INTERPRET_SLL
    gencallinterp((unsigned int)cached_interpreter_table.SLL, 0);
#else
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shl_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensllv(void)
{
#ifdef INTERPRET_SLLV
    gencallinterp((unsigned int)cached_interpreter_table.SLLV, 0);
#else
    int rt, rd;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg32_reg32(rd, rt);
        shl_reg32_cl(rd);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rt);
        shl_reg32_cl(temp);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void gendsll(void)
{
#ifdef INTERPRET_DSLL
    gencallinterp((unsigned int)cached_interpreter_table.DSLL, 0);
#else
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd1, rt1);
    mov_reg32_reg32(rd2, rt2);
    shld_reg32_reg32_imm8(rd2, rd1, g_dev.r4300.recomp.dst->f.r.sa);
    shl_reg32_imm8(rd1, g_dev.r4300.recomp.dst->f.r.sa);
    if (g_dev.r4300.recomp.dst->f.r.sa & 0x20)
    {
        mov_reg32_reg32(rd2, rd1);
        xor_reg32_reg32(rd1, rd1);
    }
#endif
}

void gendsllv(void)
{
#ifdef INTERPRET_DSLLV
    gencallinterp((unsigned int)cached_interpreter_table.DSLLV, 0);
#else
    int rt1, rt2, rd1, rd2;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd1 != ECX && rd2 != ECX)
    {
        mov_reg32_reg32(rd1, rt1);
        mov_reg32_reg32(rd2, rt2);
        shld_reg32_reg32_cl(rd2,rd1);
        shl_reg32_cl(rd1);
        test_reg32_imm32(ECX, 0x20);
        je_rj(4);
        mov_reg32_reg32(rd2, rd1); // 2
        xor_reg32_reg32(rd1, rd1); // 2
    }
    else
    {
        int temp1, temp2;
        force_32(ECX);
        temp1 = lru_register();
        temp2 = lru_register_exc1(temp1);
        free_register(temp1);
        free_register(temp2);

        mov_reg32_reg32(temp1, rt1);
        mov_reg32_reg32(temp2, rt2);
        shld_reg32_reg32_cl(temp2, temp1);
        shl_reg32_cl(temp1);
        test_reg32_imm32(ECX, 0x20);
        je_rj(4);
        mov_reg32_reg32(temp2, temp1); // 2
        xor_reg32_reg32(temp1, temp1); // 2

        mov_reg32_reg32(rd1, temp1);
        mov_reg32_reg32(rd2, temp2);
    }
#endif
}

void gendsll32(void)
{
#ifdef INTERPRET_DSLL32
    gencallinterp((unsigned int)cached_interpreter_table.DSLL32, 0);
#else
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd2, rt1);
    shl_reg32_imm8(rd2, g_dev.r4300.recomp.dst->f.r.sa);
    xor_reg32_reg32(rd1, rd1);
#endif
}

void gensrl(void)
{
#ifdef INTERPRET_SRL
    gencallinterp((unsigned int)cached_interpreter_table.SRL, 0);
#else
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shr_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensrlv(void)
{
#ifdef INTERPRET_SRLV
    gencallinterp((unsigned int)cached_interpreter_table.SRLV, 0);
#else
    int rt, rd;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg32_reg32(rd, rt);
        shr_reg32_cl(rd);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rt);
        shr_reg32_cl(temp);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void gendsrl(void)
{
#ifdef INTERPRET_DSRL
    gencallinterp((unsigned int)cached_interpreter_table.DSRL, 0);
#else
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd1, rt1);
    mov_reg32_reg32(rd2, rt2);
    shrd_reg32_reg32_imm8(rd1, rd2, g_dev.r4300.recomp.dst->f.r.sa);
    shr_reg32_imm8(rd2, g_dev.r4300.recomp.dst->f.r.sa);
    if (g_dev.r4300.recomp.dst->f.r.sa & 0x20)
    {
        mov_reg32_reg32(rd1, rd2);
        xor_reg32_reg32(rd2, rd2);
    }
#endif
}

void gendsrlv(void)
{
#ifdef INTERPRET_DSRLV
    gencallinterp((unsigned int)cached_interpreter_table.DSRLV, 0);
#else
    int rt1, rt2, rd1, rd2;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd1 != ECX && rd2 != ECX)
    {
        mov_reg32_reg32(rd1, rt1);
        mov_reg32_reg32(rd2, rt2);
        shrd_reg32_reg32_cl(rd1,rd2);
        shr_reg32_cl(rd2);
        test_reg32_imm32(ECX, 0x20);
        je_rj(4);
        mov_reg32_reg32(rd1, rd2); // 2
        xor_reg32_reg32(rd2, rd2); // 2
    }
    else
    {
        int temp1, temp2;
        force_32(ECX);
        temp1 = lru_register();
        temp2 = lru_register_exc1(temp1);
        free_register(temp1);
        free_register(temp2);

        mov_reg32_reg32(temp1, rt1);
        mov_reg32_reg32(temp2, rt2);
        shrd_reg32_reg32_cl(temp1, temp2);
        shr_reg32_cl(temp2);
        test_reg32_imm32(ECX, 0x20);
        je_rj(4);
        mov_reg32_reg32(temp1, temp2); // 2
        xor_reg32_reg32(temp2, temp2); // 2

        mov_reg32_reg32(rd1, temp1);
        mov_reg32_reg32(rd2, temp2);
    }
#endif
}

void gendsrl32(void)
{
#ifdef INTERPRET_DSRL32
    gencallinterp((unsigned int)cached_interpreter_table.DSRL32, 0);
#else
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd1, rt2);
    shr_reg32_imm8(rd1, g_dev.r4300.recomp.dst->f.r.sa);
    xor_reg32_reg32(rd2, rd2);
#endif
}

void gensra(void)
{
#ifdef INTERPRET_SRA
    gencallinterp((unsigned int)cached_interpreter_table.SRA, 0);
#else
    int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    sar_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensrav(void)
{
#ifdef INTERPRET_SRAV
    gencallinterp((unsigned int)cached_interpreter_table.SRAV, 0);
#else
    int rt, rd;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg32_reg32(rd, rt);
        sar_reg32_cl(rd);
    }
    else
    {
        int temp = lru_register();
        free_register(temp);
        mov_reg32_reg32(temp, rt);
        sar_reg32_cl(temp);
        mov_reg32_reg32(rd, temp);
    }
#endif
}

void gendsra(void)
{
#ifdef INTERPRET_DSRA
    gencallinterp((unsigned int)cached_interpreter_table.DSRA, 0);
#else
    int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd1, rt1);
    mov_reg32_reg32(rd2, rt2);
    shrd_reg32_reg32_imm8(rd1, rd2, g_dev.r4300.recomp.dst->f.r.sa);
    sar_reg32_imm8(rd2, g_dev.r4300.recomp.dst->f.r.sa);
    if (g_dev.r4300.recomp.dst->f.r.sa & 0x20)
    {
        mov_reg32_reg32(rd1, rd2);
        sar_reg32_imm8(rd2, 31);
    }
#endif
}

void gendsrav(void)
{
#ifdef INTERPRET_DSRAV
    gencallinterp((unsigned int)cached_interpreter_table.DSRAV, 0);
#else
    int rt1, rt2, rd1, rd2;
    allocate_register_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd1 = allocate_64_register1_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);
    rd2 = allocate_64_register2_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd1 != ECX && rd2 != ECX)
    {
        mov_reg32_reg32(rd1, rt1);
        mov_reg32_reg32(rd2, rt2);
        shrd_reg32_reg32_cl(rd1,rd2);
        sar_reg32_cl(rd2);
        test_reg32_imm32(ECX, 0x20);
        je_rj(5);
        mov_reg32_reg32(rd1, rd2); // 2
        sar_reg32_imm8(rd2, 31); // 3
    }
    else
    {
        int temp1, temp2;
        force_32(ECX);
        temp1 = lru_register();
        temp2 = lru_register_exc1(temp1);
        free_register(temp1);
        free_register(temp2);

        mov_reg32_reg32(temp1, rt1);
        mov_reg32_reg32(temp2, rt2);
        shrd_reg32_reg32_cl(temp1, temp2);
        sar_reg32_cl(temp2);
        test_reg32_imm32(ECX, 0x20);
        je_rj(5);
        mov_reg32_reg32(temp1, temp2); // 2
        sar_reg32_imm8(temp2, 31); // 3

        mov_reg32_reg32(rd1, temp1);
        mov_reg32_reg32(rd2, temp2);
    }
#endif
}

void gendsra32(void)
{
#ifdef INTERPRET_DSRA32
    gencallinterp((unsigned int)cached_interpreter_table.DSRA32, 0);
#else
    int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt2);
    sar_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

/* Multiply / Divide instructions */

void genmult(void)
{
#ifdef INTERPRET_MULT
    gencallinterp((unsigned int)cached_interpreter_table.MULT, 0);
#else
    int rs, rt;
    allocate_register_manually_w(EAX, (unsigned int *)r4300_mult_lo(&g_dev.r4300), 0);
    allocate_register_manually_w(EDX, (unsigned int *)r4300_mult_hi(&g_dev.r4300), 0);
    rs = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    imul_reg32(rt);
#endif
}

void genmultu(void)
{
#ifdef INTERPRET_MULTU
    gencallinterp((unsigned int)cached_interpreter_table.MULTU, 0);
#else
    int rs, rt;
    allocate_register_manually_w(EAX, (unsigned int *)r4300_mult_lo(&g_dev.r4300), 0);
    allocate_register_manually_w(EDX, (unsigned int *)r4300_mult_hi(&g_dev.r4300), 0);
    rs = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    mul_reg32(rt);
#endif
}

void gendmult(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.DMULT, 0);
}

void gendmultu(void)
{
#ifdef INTERPRET_DMULTU
    gencallinterp((unsigned int)cached_interpreter_table.DMULTU, 0);
#else
    free_all_registers();
    simplify_access();

    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    mul_m32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt); // EDX:EAX = temp1
    mov_memoffs32_eax((unsigned int *)(r4300_mult_lo(&g_dev.r4300)));

    mov_reg32_reg32(EBX, EDX); // EBX = temp1>>32
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    mul_m32((unsigned int *)(g_dev.r4300.recomp.dst->f.r.rt)+1);
    add_reg32_reg32(EBX, EAX);
    adc_reg32_imm32(EDX, 0);
    mov_reg32_reg32(ECX, EDX); // ECX:EBX = temp2

    mov_eax_memoffs32((unsigned int *)(g_dev.r4300.recomp.dst->f.r.rs)+1);
    mul_m32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt); // EDX:EAX = temp3

    add_reg32_reg32(EBX, EAX);
    adc_reg32_imm32(ECX, 0); // ECX:EBX = result2
    mov_m32_reg32((unsigned int*)(r4300_mult_lo(&g_dev.r4300))+1, EBX);

    mov_reg32_reg32(ESI, EDX); // ESI = temp3>>32
    mov_eax_memoffs32((unsigned int *)(g_dev.r4300.recomp.dst->f.r.rs)+1);
    mul_m32((unsigned int *)(g_dev.r4300.recomp.dst->f.r.rt)+1);
    add_reg32_reg32(EAX, ESI);
    adc_reg32_imm32(EDX, 0); // EDX:EAX = temp4

    add_reg32_reg32(EAX, ECX);
    adc_reg32_imm32(EDX, 0); // EDX:EAX = result3
    mov_memoffs32_eax((unsigned int *)(r4300_mult_hi(&g_dev.r4300)));
    mov_m32_reg32((unsigned int *)(r4300_mult_hi(&g_dev.r4300))+1, EDX);
#endif
}

void gendiv(void)
{
#ifdef INTERPRET_DIV
    gencallinterp((unsigned int)cached_interpreter_table.DIV, 0);
#else
    int rs, rt;
    allocate_register_manually_w(EAX, (unsigned int *)r4300_mult_lo(&g_dev.r4300), 0);
    allocate_register_manually_w(EDX, (unsigned int *)r4300_mult_hi(&g_dev.r4300), 0);
    rs = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 1 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    cdq(); // 1
    idiv_reg32(rt); // 2
#endif
}

void gendivu(void)
{
#ifdef INTERPRET_DIVU
    gencallinterp((unsigned int)cached_interpreter_table.DIVU, 0);
#else
    int rs, rt;
    allocate_register_manually_w(EAX, (unsigned int *)r4300_mult_lo(&g_dev.r4300), 0);
    allocate_register_manually_w(EDX, (unsigned int *)r4300_mult_hi(&g_dev.r4300), 0);
    rs = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 2 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    xor_reg32_reg32(EDX, EDX); // 2
    div_reg32(rt); // 2
#endif
}

void genddiv(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.DDIV, 0);
}

void genddivu(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.DDIVU, 0);
}

void genmfhi(void)
{
#ifdef INTERPRET_MFHI
    gencallinterp((unsigned int)cached_interpreter_table.MFHI, 0);
#else
    int rd1 = allocate_64_register1_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rd);
    int hi1 = allocate_64_register1((unsigned int*)r4300_mult_hi(&g_dev.r4300));
    int hi2 = allocate_64_register2((unsigned int*)r4300_mult_hi(&g_dev.r4300));

    mov_reg32_reg32(rd1, hi1);
    mov_reg32_reg32(rd2, hi2);
#endif
}

void genmthi(void)
{
#ifdef INTERPRET_MTHI
    gencallinterp((unsigned int)cached_interpreter_table.MTHI, 0);
#else
    int hi1 = allocate_64_register1_w((unsigned int*)r4300_mult_hi(&g_dev.r4300));
    int hi2 = allocate_64_register2_w((unsigned int*)r4300_mult_hi(&g_dev.r4300));
    int rs1 = allocate_64_register1((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);

    mov_reg32_reg32(hi1, rs1);
    mov_reg32_reg32(hi2, rs2);
#endif
}

void genmflo(void)
{
#ifdef INTERPRET_MFLO
    gencallinterp((unsigned int)cached_interpreter_table.MFLO, 0);
#else
    int rd1 = allocate_64_register1_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rd);
    int rd2 = allocate_64_register2_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rd);
    int lo1 = allocate_64_register1((unsigned int*)r4300_mult_lo(&g_dev.r4300));
    int lo2 = allocate_64_register2((unsigned int*)r4300_mult_lo(&g_dev.r4300));

    mov_reg32_reg32(rd1, lo1);
    mov_reg32_reg32(rd2, lo2);
#endif
}

void genmtlo(void)
{
#ifdef INTERPRET_MTLO
    gencallinterp((unsigned int)cached_interpreter_table.MTLO, 0);
#else
    int lo1 = allocate_64_register1_w((unsigned int*)r4300_mult_lo(&g_dev.r4300));
    int lo2 = allocate_64_register2_w((unsigned int*)r4300_mult_lo(&g_dev.r4300));
    int rs1 = allocate_64_register1((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    int rs2 = allocate_64_register2((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);

    mov_reg32_reg32(lo1, rs1);
    mov_reg32_reg32(lo2, rs2);
#endif
}

/* Jump & Branch instructions */

static void gentest(void)
{
    cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + (g_dev.r4300.recomp.dst-1)->f.i.immediate));
    jmp(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentest_out(void)
{
    cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);

    jump_end_rel32();

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentest_idle(void)
{
    int reg;

    reg = lru_register();
    free_register(reg);

    cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);

    jump_start_rel32();

    mov_reg32_m32(reg, (unsigned int *)(r4300_cp0_next_interrupt(&g_dev.r4300.cp0)));
    sub_reg32_m32(reg, (unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(reg, 5);
    jbe_rj(18);

    sub_reg32_imm32(reg, 2); // 6
    and_reg32_imm32(reg, 0xFFFFFFFC); // 6
    add_m32_reg32((unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]), reg); // 6

    jump_end_rel32();
}

static void gentestl(void)
{
    cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);

    jump_start_rel32();

    gendelayslot();
    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + (g_dev.r4300.recomp.dst-1)->f.i.immediate));
    jmp(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    gencp0_update_count(g_dev.r4300.recomp.dst->addr+4);
    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentestl_out(void)
{
    cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);

    jump_start_rel32();

    gendelayslot();
    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);

    jump_end_rel32();

    gencp0_update_count(g_dev.r4300.recomp.dst->addr+4);
    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void genbranchlink(void)
{
    int r31_64bit = is64((unsigned int*)&r4300_regs(&g_dev.r4300)[31]);

    if (!r31_64bit)
    {
        int r31 = allocate_register_w((unsigned int *)&r4300_regs(&g_dev.r4300)[31]);

        mov_reg32_imm32(r31, g_dev.r4300.recomp.dst->addr+8);
    }
    else if (r31_64bit == -1)
    {
        mov_m32_imm32((unsigned int *)&r4300_regs(&g_dev.r4300)[31], g_dev.r4300.recomp.dst->addr + 8);
        if (g_dev.r4300.recomp.dst->addr & 0x80000000) {
            mov_m32_imm32(((unsigned int *)&r4300_regs(&g_dev.r4300)[31])+1, 0xFFFFFFFF);
        }
        else {
            mov_m32_imm32(((unsigned int *)&r4300_regs(&g_dev.r4300)[31])+1, 0);
        }
    }
    else
    {
        int r311 = allocate_64_register1_w((unsigned int *)&r4300_regs(&g_dev.r4300)[31]);
        int r312 = allocate_64_register2_w((unsigned int *)&r4300_regs(&g_dev.r4300)[31]);

        mov_reg32_imm32(r311, g_dev.r4300.recomp.dst->addr+8);
        if (g_dev.r4300.recomp.dst->addr & 0x80000000) {
            mov_reg32_imm32(r312, 0xFFFFFFFF);
        }
        else {
            mov_reg32_imm32(r312, 0);
        }
    }
}

void genj(void)
{
#ifdef INTERPRET_J
    gencallinterp((unsigned int)cached_interpreter_table.J, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.J, 1);
        return;
    }

    gendelayslot();
    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
    gencheck_interrupt((unsigned int)&g_dev.r4300.cached_interp.actual->block[(naddr-g_dev.r4300.cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genj_out(void)
{
#ifdef INTERPRET_J_OUT
    gencallinterp((unsigned int)cached_interpreter_table.J_OUT, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.J_OUT, 1);
        return;
    }

    gendelayslot();
    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
    gencheck_interrupt_out(naddr);
    mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);
#endif
}

void genj_idle(void)
{
#ifdef INTERPRET_J_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.J_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.J_IDLE, 1);
        return;
    }

    mov_eax_memoffs32((unsigned int *)(r4300_cp0_next_interrupt(&g_dev.r4300.cp0)));
    sub_reg32_m32(EAX, (unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(11);

    and_eax_imm32(0xFFFFFFFC);  // 5
    add_m32_reg32((unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]), EAX); // 6

    genj();
#endif
}

void genjal(void)
{
#ifdef INTERPRET_JAL
    gencallinterp((unsigned int)cached_interpreter_table.JAL, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.JAL, 1);
        return;
    }

    gendelayslot();

    mov_m32_imm32((unsigned int *)(r4300_regs(&g_dev.r4300) + 31), g_dev.r4300.recomp.dst->addr + 4);
    if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32_imm32((unsigned int *)(&r4300_regs(&g_dev.r4300)[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32_imm32((unsigned int *)(&r4300_regs(&g_dev.r4300)[31])+1, 0);
    }

    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
    gencheck_interrupt((unsigned int)&g_dev.r4300.cached_interp.actual->block[(naddr-g_dev.r4300.cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genjal_out(void)
{
#ifdef INTERPRET_JAL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.JAL_OUT, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.JAL_OUT, 1);
        return;
    }

    gendelayslot();

    mov_m32_imm32((unsigned int *)(r4300_regs(&g_dev.r4300) + 31), g_dev.r4300.recomp.dst->addr + 4);
    if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32_imm32((unsigned int *)(&r4300_regs(&g_dev.r4300)[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32_imm32((unsigned int *)(&r4300_regs(&g_dev.r4300)[31])+1, 0);
    }

    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
    gencheck_interrupt_out(naddr);
    mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);
#endif
}

void genjal_idle(void)
{
#ifdef INTERPRET_JAL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.JAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.JAL_IDLE, 1);
        return;
    }

    mov_eax_memoffs32((unsigned int *)(r4300_cp0_next_interrupt(&g_dev.r4300.cp0)));
    sub_reg32_m32(EAX, (unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(11);

    and_eax_imm32(0xFFFFFFFC);
    add_m32_reg32((unsigned int *)(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_COUNT_REG]), EAX);

    genjal();
#endif
}

void genjr(void)
{
#ifdef INTERPRET_JR
    gencallinterp((unsigned int)cached_interpreter_table.JR, 1);
#else
    unsigned int diff =
        (unsigned int)(&g_dev.r4300.recomp.dst->local_addr) - (unsigned int)(g_dev.r4300.recomp.dst);
    unsigned int diff_need =
        (unsigned int)(&g_dev.r4300.recomp.dst->reg_cache_infos.need_map) - (unsigned int)(g_dev.r4300.recomp.dst);
    unsigned int diff_wrap =
        (unsigned int)(&g_dev.r4300.recomp.dst->reg_cache_infos.jump_wrapper) - (unsigned int)(g_dev.r4300.recomp.dst);

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.JR, 1);
        return;
    }

    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    mov_memoffs32_eax((unsigned int *)&g_dev.r4300.local_rs);

    gendelayslot();

    mov_eax_memoffs32((unsigned int *)&g_dev.r4300.local_rs);
    mov_memoffs32_eax((unsigned int *)&g_dev.r4300.cp0.last_addr);

    gencheck_interrupt_reg();

    mov_eax_memoffs32((unsigned int *)&g_dev.r4300.local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(g_dev.r4300.recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32_reg32(&g_dev.r4300.recomp.jump_to_address, EBX);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);

    jump_end_rel32();

    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(g_dev.r4300.recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg32pimm32(EBX, EAX, (unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(7);

    add_eax_imm32((unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff_wrap); // 5
    jmp_reg32(EAX); // 2

    mov_reg32_preg32pimm32(EAX, EAX, (unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff);
    add_reg32_m32(EAX, (unsigned int *)(&g_dev.r4300.recomp.dst_block->code));

    jmp_reg32(EAX);
#endif
}

void genjalr(void)
{
#ifdef INTERPRET_JALR
    gencallinterp((unsigned int)cached_interpreter_table.JALR, 0);
#else
    unsigned int diff =
        (unsigned int)(&g_dev.r4300.recomp.dst->local_addr) - (unsigned int)(g_dev.r4300.recomp.dst);
    unsigned int diff_need =
        (unsigned int)(&g_dev.r4300.recomp.dst->reg_cache_infos.need_map) - (unsigned int)(g_dev.r4300.recomp.dst);
    unsigned int diff_wrap =
        (unsigned int)(&g_dev.r4300.recomp.dst->reg_cache_infos.jump_wrapper) - (unsigned int)(g_dev.r4300.recomp.dst);

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.JALR, 1);
        return;
    }

    free_all_registers();
    simplify_access();
    mov_eax_memoffs32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    mov_memoffs32_eax((unsigned int *)&g_dev.r4300.local_rs);

    gendelayslot();

    mov_m32_imm32((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd, g_dev.r4300.recomp.dst->addr+4);
    if ((g_dev.r4300.recomp.dst->addr+4) & 0x80000000) {
        mov_m32_imm32(((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd)+1, 0xFFFFFFFF);
    }
    else {
        mov_m32_imm32(((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd)+1, 0);
    }

    mov_eax_memoffs32((unsigned int *)&g_dev.r4300.local_rs);
    mov_memoffs32_eax((unsigned int *)&g_dev.r4300.cp0.last_addr);

    gencheck_interrupt_reg();

    mov_eax_memoffs32((unsigned int *)&g_dev.r4300.local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(g_dev.r4300.recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32_reg32(&g_dev.r4300.recomp.jump_to_address, EBX);
    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1));
    mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
    call_reg32(EAX);

    jump_end_rel32();

    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(g_dev.r4300.recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg32pimm32(EBX, EAX, (unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(7);

    add_eax_imm32((unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff_wrap); // 5
    jmp_reg32(EAX); // 2

    mov_reg32_preg32pimm32(EAX, EAX, (unsigned int)(g_dev.r4300.recomp.dst_block->block)+diff);
    add_reg32_m32(EAX, (unsigned int *)(&g_dev.r4300.recomp.dst_block->code));

    jmp_reg32(EAX);
#endif
}

static void genbeq_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    if (!rs_64bit && !rt_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        jne_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_m32(rt1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        jne_rj(20);
        cmp_reg32_m32(rt2, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1); // 6
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rt_64bit == -1)
    {
        int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_m32(rs1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        jne_rj(20);
        cmp_reg32_m32(rs2, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt)+1); // 6
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else
    {
        int rs1, rs2, rt1, rt2;
        if (!rs_64bit)
        {
            rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        }
        else
        {
            rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        }
        cmp_reg32_reg32(rs1, rt1);
        jne_rj(16);
        cmp_reg32_reg32(rs2, rt2); // 2
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
}

void genbeq(void)
{
#ifdef INTERPRET_BEQ
    gencallinterp((unsigned int)cached_interpreter_table.BEQ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQ, 1);
        return;
    }

    genbeq_test();
    gendelayslot();
    gentest();
#endif
}

void genbeq_out(void)
{
#ifdef INTERPRET_BEQ_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BEQ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQ_OUT, 1);
        return;
    }

    genbeq_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbeq_idle(void)
{
#ifdef INTERPRET_BEQ_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BEQ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQ_IDLE, 1);
        return;
    }

    genbeq_test();
    gentest_idle();
    genbeq();
#endif
}

void genbeql(void)
{
#ifdef INTERPRET_BEQL
    gencallinterp((unsigned int)cached_interpreter_table.BEQL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQL, 1);
        return;
    }

    genbeq_test();
    free_all_registers();
    gentestl();
#endif
}

void genbeql_out(void)
{
#ifdef INTERPRET_BEQL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BEQL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQL_OUT, 1);
        return;
    }

    genbeq_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbeql_idle(void)
{
#ifdef INTERPRET_BEQL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BEQL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BEQL_IDLE, 1);
        return;
    }

    genbeq_test();
    gentest_idle();
    genbeql();
#endif
}

static void genbne_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    if (!rs_64bit && !rt_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        je_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        int rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        int rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_m32(rt1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        jne_rj(20);
        cmp_reg32_m32(rt2, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1); // 6
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
    else if (rt_64bit == -1)
    {
        int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_m32(rs1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        jne_rj(20);
        cmp_reg32_m32(rs2, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt)+1); // 6
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
    else
    {
        int rs1, rs2, rt1, rt2;
        if (!rs_64bit)
        {
            rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        }
        else
        {
            rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
            rt1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
            rt2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
        }
        cmp_reg32_reg32(rs1, rt1);
        jne_rj(16);
        cmp_reg32_reg32(rs2, rt2); // 2
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
}

void genbne(void)
{
#ifdef INTERPRET_BNE
    gencallinterp((unsigned int)cached_interpreter_table.BNE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNE, 1);
        return;
    }

    genbne_test();
    gendelayslot();
    gentest();
#endif
}

void genbne_out(void)
{
#ifdef INTERPRET_BNE_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BNE_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNE_OUT, 1);
        return;
    }

    genbne_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbne_idle(void)
{
#ifdef INTERPRET_BNE_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BNE_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNE_IDLE, 1);
        return;
    }

    genbne_test();
    gentest_idle();
    genbne();
#endif
}

void genbnel(void)
{
#ifdef INTERPRET_BNEL
    gencallinterp((unsigned int)cached_interpreter_table.BNEL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNEL, 1);
        return;
    }

    genbne_test();
    free_all_registers();
    gentestl();
#endif
}

void genbnel_out(void)
{
#ifdef INTERPRET_BNEL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BNEL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNEL_OUT, 1);
        return;
    }

    genbne_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbnel_idle(void)
{
#ifdef INTERPRET_BNEL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BNEL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BNEL_IDLE, 1);
        return;
    }

    genbne_test();
    gentest_idle();
    genbnel();
#endif
}

static void genblez_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

    if (!rs_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        jg_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        cmp_m32_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        jg_rj(14);
        jne_rj(24); // 2
        cmp_m32_imm32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs, 0); // 10
        je_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
    else
    {
        int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs2, 0);
        jg_rj(10);
        jne_rj(20); // 2
        cmp_reg32_imm32(rs1, 0); // 6
        je_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
}

void genblez(void)
{
#ifdef INTERPRET_BLEZ
    gencallinterp((unsigned int)cached_interpreter_table.BLEZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZ, 1);
        return;
    }

    genblez_test();
    gendelayslot();
    gentest();
#endif
}

void genblez_out(void)
{
#ifdef INTERPRET_BLEZ_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLEZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZ_OUT, 1);
        return;
    }

    genblez_test();
    gendelayslot();
    gentest_out();
#endif
}

void genblez_idle(void)
{
#ifdef INTERPRET_BLEZ_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLEZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZ_IDLE, 1);
        return;
    }

    genblez_test();
    gentest_idle();
    genblez();
#endif
}

void genblezl(void)
{
#ifdef INTERPRET_BLEZL
    gencallinterp((unsigned int)cached_interpreter_table.BLEZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZL, 1);
        return;
    }

    genblez_test();
    free_all_registers();
    gentestl();
#endif
}

void genblezl_out(void)
{
#ifdef INTERPRET_BLEZL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLEZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZL_OUT, 1);
        return;
    }

    genblez_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genblezl_idle(void)
{
#ifdef INTERPRET_BLEZL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLEZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLEZL_IDLE, 1);
        return;
    }

    genblez_test();
    gentest_idle();
    genblezl();
#endif
}

static void genbgtz_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

    if (!rs_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        jle_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        cmp_m32_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        jl_rj(14);
        jne_rj(24); // 2
        cmp_m32_imm32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs, 0); // 10
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
    else
    {
        int rs1 = allocate_64_register1((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs2, 0);
        jl_rj(10);
        jne_rj(20); // 2
        cmp_reg32_imm32(rs1, 0); // 6
        jne_rj(12); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
    }
}

void genbgtz(void)
{
#ifdef INTERPRET_BGTZ
    gencallinterp((unsigned int)cached_interpreter_table.BGTZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZ, 1);
        return;
    }

    genbgtz_test();
    gendelayslot();
    gentest();
#endif
}

void genbgtz_out(void)
{
#ifdef INTERPRET_BGTZ_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGTZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZ_OUT, 1);
        return;
    }

    genbgtz_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbgtz_idle(void)
{
#ifdef INTERPRET_BGTZ_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGTZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZ_IDLE, 1);
        return;
    }

    genbgtz_test();
    gentest_idle();
    genbgtz();
#endif
}

void genbgtzl(void)
{
#ifdef INTERPRET_BGTZL
    gencallinterp((unsigned int)cached_interpreter_table.BGTZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZL, 1);
        return;
    }

    genbgtz_test();
    free_all_registers();
    gentestl();
#endif
}

void genbgtzl_out(void)
{
#ifdef INTERPRET_BGTZL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGTZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZL_OUT, 1);
        return;
    }

    genbgtz_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbgtzl_idle(void)
{
#ifdef INTERPRET_BGTZL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGTZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGTZL_IDLE, 1);
        return;
    }

    genbgtz_test();
    gentest_idle();
    genbgtzl();
#endif
}

static void genbltz_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

    if (!rs_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        jge_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        cmp_m32_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        jge_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else
    {
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs2, 0);
        jge_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
}

void genbltz(void)
{
#ifdef INTERPRET_BLTZ
    gencallinterp((unsigned int)cached_interpreter_table.BLTZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZ, 1);
        return;
    }

    genbltz_test();
    gendelayslot();
    gentest();
#endif
}

void genbltz_out(void)
{
#ifdef INTERPRET_BLTZ_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLTZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZ_OUT, 1);
        return;
    }

    genbltz_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbltz_idle(void)
{
#ifdef INTERPRET_BLTZ_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLTZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZ_IDLE, 1);
        return;
    }

    genbltz_test();
    gentest_idle();
    genbltz();
#endif
}

void genbltzal(void)
{
#ifdef INTERPRET_BLTZAL
    gencallinterp((unsigned int)cached_interpreter_table.BLTZAL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZAL, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    gendelayslot();
    gentest();
#endif
}

void genbltzal_out(void)
{
#ifdef INTERPRET_BLTZAL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLTZAL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZAL_OUT, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    gendelayslot();
    gentest_out();
#endif
}

void genbltzal_idle(void)
{
#ifdef INTERPRET_BLTZAL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLTZAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZAL_IDLE, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    gentest_idle();
    genbltzal();
#endif
}

void genbltzl(void)
{
#ifdef INTERPRET_BLTZL
    gencallinterp((unsigned int)cached_interpreter_table.BLTZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZL, 1);
        return;
    }

    genbltz_test();
    free_all_registers();
    gentestl();
#endif
}

void genbltzl_out(void)
{
#ifdef INTERPRET_BLTZL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLTZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZL_OUT, 1);
        return;
    }

    genbltz_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbltzl_idle(void)
{
#ifdef INTERPRET_BLTZL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLTZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZL_IDLE, 1);
        return;
    }

    genbltz_test();
    gentest_idle();
    genbltzl();
#endif
}

void genbltzall(void)
{
#ifdef INTERPRET_BLTZALL
    gencallinterp((unsigned int)cached_interpreter_table.BLTZALL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZALL, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    free_all_registers();
    gentestl();
#endif
}

void genbltzall_out(void)
{
#ifdef INTERPRET_BLTZALL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BLTZALL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZALL_OUT, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    free_all_registers();
    gentestl_out();
#endif
}

void genbltzall_idle(void)
{
#ifdef INTERPRET_BLTZALL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BLTZALL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BLTZALL_IDLE, 1);
        return;
    }

    genbltz_test();
    genbranchlink();
    gentest_idle();
    genbltzall();
#endif
}

static void genbgez_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

    if (!rs_64bit)
    {
        int rs = allocate_register((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        jl_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else if (rs_64bit == -1)
    {
        cmp_m32_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        jl_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
    else
    {
        int rs2 = allocate_64_register2((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs2, 0);
        jl_rj(12);
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 1); // 10
        jmp_imm_short(10); // 2
        mov_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0); // 10
    }
}

void genbgez(void)
{
#ifdef INTERPRET_BGEZ
    gencallinterp((unsigned int)cached_interpreter_table.BGEZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZ, 1);
        return;
    }

    genbgez_test();
    gendelayslot();
    gentest();
#endif
}

void genbgez_out(void)
{
#ifdef INTERPRET_BGEZ_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGEZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZ_OUT, 1);
        return;
    }

    genbgez_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbgez_idle(void)
{
#ifdef INTERPRET_BGEZ_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGEZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZ_IDLE, 1);
        return;
    }

    genbgez_test();
    gentest_idle();
    genbgez();
#endif
}

void genbgezal(void)
{
#ifdef INTERPRET_BGEZAL
    gencallinterp((unsigned int)cached_interpreter_table.BGEZAL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZAL, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    gendelayslot();
    gentest();
#endif
}

void genbgezal_out(void)
{
#ifdef INTERPRET_BGEZAL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGEZAL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZAL_OUT, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    gendelayslot();
    gentest_out();
#endif
}

void genbgezal_idle(void)
{
#ifdef INTERPRET_BGEZAL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGEZAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZAL_IDLE, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    gentest_idle();
    genbgezal();
#endif
}

void genbgezl(void)
{
#ifdef INTERPRET_BGEZL
    gencallinterp((unsigned int)cached_interpreter_table.BGEZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZL, 1);
        return;
    }

    genbgez_test();
    free_all_registers();
    gentestl();
#endif
}

void genbgezl_out(void)
{
#ifdef INTERPRET_BGEZL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGEZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZL_OUT, 1);
        return;
    }

    genbgez_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbgezl_idle(void)
{
#ifdef INTERPRET_BGEZL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGEZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZL_IDLE, 1);
        return;
    }

    genbgez_test();
    gentest_idle();
    genbgezl();
#endif
}

void genbgezall(void)
{
#ifdef INTERPRET_BGEZALL
    gencallinterp((unsigned int)cached_interpreter_table.BGEZALL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZALL, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    free_all_registers();
    gentestl();
#endif
}

void genbgezall_out(void)
{
#ifdef INTERPRET_BGEZALL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BGEZALL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZALL_OUT, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    free_all_registers();
    gentestl_out();
#endif
}

void genbgezall_idle(void)
{
#ifdef INTERPRET_BGEZALL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BGEZALL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BGEZALL_IDLE, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    gentest_idle();
    genbgezall();
#endif
}

static void genbc1f_test(void)
{
    test_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000);
    jne_rj(12);
    mov_m32_imm32((unsigned int*)(&g_dev.r4300.branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int*)(&g_dev.r4300.branch_taken), 0); // 10
}

void genbc1f(void)
{
#ifdef INTERPRET_BC1F
    gencallinterp((unsigned int)cached_interpreter_table.BC1F, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1F, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    gendelayslot();
    gentest();
#endif
}

void genbc1f_out(void)
{
#ifdef INTERPRET_BC1F_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BC1F_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1F_OUT, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbc1f_idle(void)
{
#ifdef INTERPRET_BC1F_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BC1F_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1F_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    gentest_idle();
    genbc1f();
#endif
}

void genbc1fl(void)
{
#ifdef INTERPRET_BC1FL
    gencallinterp((unsigned int)cached_interpreter_table.BC1FL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1FL, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    free_all_registers();
    gentestl();
#endif
}

void genbc1fl_out(void)
{
#ifdef INTERPRET_BC1FL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BC1FL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1FL_OUT, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbc1fl_idle(void)
{
#ifdef INTERPRET_BC1FL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BC1FL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1FL_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1f_test();
    gentest_idle();
    genbc1fl();
#endif
}

static void genbc1t_test(void)
{
    test_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000);
    je_rj(12);
    mov_m32_imm32((unsigned int*)(&g_dev.r4300.branch_taken), 1); // 10
    jmp_imm_short(10); // 2
    mov_m32_imm32((unsigned int*)(&g_dev.r4300.branch_taken), 0); // 10
}

void genbc1t(void)
{
#ifdef INTERPRET_BC1T
    gencallinterp((unsigned int)cached_interpreter_table.BC1T, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1T, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    gendelayslot();
    gentest();
#endif
}

void genbc1t_out(void)
{
#ifdef INTERPRET_BC1T_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BC1T_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1T_OUT, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    gendelayslot();
    gentest_out();
#endif
}

void genbc1t_idle(void)
{
#ifdef INTERPRET_BC1T_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BC1T_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1T_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    gentest_idle();
    genbc1t();
#endif
}

void genbc1tl(void)
{
#ifdef INTERPRET_BC1TL
    gencallinterp((unsigned int)cached_interpreter_table.BC1TL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1TL, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    free_all_registers();
    gentestl();
#endif
}

void genbc1tl_out(void)
{
#ifdef INTERPRET_BC1TL_OUT
    gencallinterp((unsigned int)cached_interpreter_table.BC1TL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1TL_OUT, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    free_all_registers();
    gentestl_out();
#endif
}

void genbc1tl_idle(void)
{
#ifdef INTERPRET_BC1TL_IDLE
    gencallinterp((unsigned int)cached_interpreter_table.BC1TL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned int)cached_interpreter_table.BC1TL_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable();
    genbc1t_test();
    gentest_idle();
    genbc1tl();
#endif
}

/* Special instructions */

void gencache(void)
{
}

void generet(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.ERET, 1);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(dst));
    genupdate_system(0);
    mov_reg32_imm32(EAX, (unsigned int)(ERET));
    call_reg32(EAX);
    mov_reg32_imm32(EAX, (unsigned int)(jump_code));
    jmp_reg32(EAX);
#endif
}

void gensync(void)
{
}

void gensyscall(void)
{
#ifdef INTERPRET_SYSCALL
    gencallinterp((unsigned int)cached_interpreter_table.SYSCALL, 0);
#else
    free_all_registers();
    simplify_access();
    mov_m32_imm32(&r4300_cp0_regs(&g_dev.r4300.cp0)[CP0_CAUSE_REG], 8 << 2);
    gencallinterp((unsigned int)dynarec_exception_general, 0);
#endif
}

/* Exception instructions */

void genteq(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.TEQ, 0);
}

/* TLB instructions */

void gentlbp(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.TLBP, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBP));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

void gentlbr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.TLBR, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBR));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

void gentlbwr(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.TLBWR, 0);
}

void gentlbwi(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.TLBWI, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBWI));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

/* CP0 load/store instructions */

void genmfc0(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.MFC0, 0);
}

void genmtc0(void)
{
    gencallinterp((unsigned int)cached_interpreter_table.MTC0, 0);
}

/* CP1 load/store instructions */

void genlwc1(void)
{
#ifdef INTERPRET_LWC1
    gencallinterp((unsigned int)cached_interpreter_table.LWC1, 0);
#else
    gencheck_cop1_unusable();

    mov_eax_memoffs32((unsigned int *)(&r4300_regs(&g_dev.r4300)[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmem);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdram);
    }
    je_rj(42);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
    mov_m32_reg32((unsigned int *)(&g_dev.r4300.rdword), EDX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmem); // 7
    call_reg32(EBX); // 2
    jmp_imm_short(20); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6
    mov_reg32_m32(EBX, (unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
    mov_preg32_reg32(EBX, EAX); // 2
#endif
}

void genldc1(void)
{
#ifdef INTERPRET_LDC1
    gencallinterp((unsigned int)cached_interpreter_table.LDC1, 0);
#else
    gencheck_cop1_unusable();

    mov_eax_memoffs32((unsigned int *)(&r4300_regs(&g_dev.r4300)[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.readmemd);
        cmp_reg32_imm32(EAX, (unsigned int)read_rdramd);
    }
    je_rj(42);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
    mov_m32_reg32((unsigned int *)(&g_dev.r4300.rdword), EDX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemd); // 7
    call_reg32(EBX); // 2
    jmp_imm_short(32); // 2

    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg32pimm32(EAX, EBX, ((unsigned int)g_dev.ri.rdram.dram)+4); // 6
    mov_reg32_preg32pimm32(ECX, EBX, ((unsigned int)g_dev.ri.rdram.dram)); // 6
    mov_reg32_m32(EBX, (unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
    mov_preg32_reg32(EBX, EAX); // 2
    mov_preg32pimm32_reg32(EBX, 4, ECX); // 6
#endif
}

void genswc1(void)
{
#ifdef INTERPRET_SWC1
    gencallinterp((unsigned int)cached_interpreter_table.SWC1, 0);
#else
    gencheck_cop1_unusable();

    mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft]));
    mov_reg32_preg32(ECX, EDX);
    mov_eax_memoffs32((unsigned int *)(&r4300_regs(&g_dev.r4300)[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writemem);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdram);
    }
    je_rj(41);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wword(&g_dev.r4300)), ECX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writemem); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(14); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg32pimm32_reg32(EBX, (unsigned int)g_dev.ri.rdram.dram, ECX); // 6

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void gensdc1(void)
{
#ifdef INTERPRET_SDC1
    gencallinterp((unsigned int)cached_interpreter_table.SDC1, 0);
#else
    gencheck_cop1_unusable();

    mov_reg32_m32(ESI, (unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.lf.ft]));
    mov_reg32_preg32(ECX, ESI);
    mov_reg32_preg32pimm32(EDX, ESI, 4);
    mov_eax_memoffs32((unsigned int *)(&r4300_regs(&g_dev.r4300)[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        shr_reg32_imm8(EAX, 16);
        mov_reg32_preg32x4pimm32(EAX, EAX, (unsigned int)g_dev.mem.writememd);
        cmp_reg32_imm32(EAX, (unsigned int)write_rdramd);
    }
    je_rj(47);

    mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct(&g_dev.r4300))), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
    mov_m32_reg32((unsigned int *)(r4300_address(&g_dev.r4300)), EBX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wdword(&g_dev.r4300)), ECX); // 6
    mov_m32_reg32((unsigned int *)(r4300_wdword(&g_dev.r4300))+1, EDX); // 6
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememd); // 7
    call_reg32(EBX); // 2
    mov_eax_memoffs32((unsigned int *)(r4300_address(&g_dev.r4300))); // 5
    jmp_imm_short(20); // 2

    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg32pimm32_reg32(EBX, ((unsigned int)g_dev.ri.rdram.dram)+4, ECX); // 6
    mov_preg32pimm32_reg32(EBX, ((unsigned int)g_dev.ri.rdram.dram)+0, EDX); // 6

    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg32pimm32_imm8(EBX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 0);
    jne_rj(54);
    mov_reg32_reg32(ECX, EBX); // 2
    shl_reg32_imm8(EBX, 2); // 3
    mov_reg32_preg32pimm32(EBX, EBX, (unsigned int)g_dev.r4300.cached_interp.blocks); // 6
    mov_reg32_preg32pimm32(EBX, EBX, (int)&g_dev.r4300.cached_interp.actual->block - (int)g_dev.r4300.cached_interp.actual); // 6
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg32_preg32preg32pimm32(EAX, EAX, EBX, (int)&g_dev.r4300.recomp.dst->ops - (int)g_dev.r4300.recomp.dst); // 7
    cmp_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED); // 6
    je_rj(7); // 2
    mov_preg32pimm32_imm8(ECX, (unsigned int)g_dev.r4300.cached_interp.invalid_code, 1); // 7
#endif
}

void genmfc1(void)
{
#ifdef INTERPRET_MFC1
    gencallinterp((unsigned int)cached_interpreter_table.MFC1, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_reg32_preg32(EBX, EAX);
    mov_m32_reg32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt, EBX);
    sar_reg32_imm8(EBX, 31);
    mov_m32_reg32(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1, EBX);
#endif
}

void gendmfc1(void)
{
#ifdef INTERPRET_DMFC1
    gencallinterp((unsigned int)cached_interpreter_table.DMFC1, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_reg32_preg32(EBX, EAX);
    mov_reg32_preg32pimm32(ECX, EAX, 4);
    mov_m32_reg32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt, EBX);
    mov_m32_reg32(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1, ECX);
#endif
}

void gencfc1(void)
{
#ifdef INTERPRET_CFC1
    gencallinterp((unsigned int)cached_interpreter_table.CFC1, 0);
#else
    gencheck_cop1_unusable();
    if (g_dev.r4300.recomp.dst->f.r.nrd == 31) {
        mov_eax_memoffs32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)));
    }
    else {
        mov_eax_memoffs32((unsigned int*)&(*r4300_cp1_fcr0(&g_dev.r4300.cp1)));
    }
    mov_memoffs32_eax((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    sar_reg32_imm8(EAX, 31);
    mov_memoffs32_eax(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1);
#endif
}

void genmtc1(void)
{
#ifdef INTERPRET_MTC1
    gencallinterp((unsigned int)cached_interpreter_table.MTC1, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_m32(EBX, (unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_preg32_reg32(EBX, EAX);
#endif
}

void gendmtc1(void)
{
#ifdef INTERPRET_DMTC1
    gencallinterp((unsigned int)cached_interpreter_table.DMTC1, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_m32(EBX, ((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1);
    mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_preg32_reg32(EDX, EAX);
    mov_preg32pimm32_reg32(EDX, 4, EBX);
#endif
}

void genctc1(void)
{
#ifdef INTERPRET_CTC1
    gencallinterp((unsigned int)cached_interpreter_table.CTC1, 0);
#else
    gencheck_cop1_unusable();

    if (g_dev.r4300.recomp.dst->f.r.nrd != 31) {
        return;
    }
    mov_eax_memoffs32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_memoffs32_eax((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)));
    and_eax_imm32(3);

    cmp_eax_imm32(0);
    jne_rj(12);
    mov_m32_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0x33F); // 10
    jmp_imm_short(48); // 2

    cmp_eax_imm32(1); // 5
    jne_rj(12); // 2
    mov_m32_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0xF3F); // 10
    jmp_imm_short(29); // 2

    cmp_eax_imm32(2); // 5
    jne_rj(12); // 2
    mov_m32_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0xB3F); // 10
    jmp_imm_short(10); // 2

    mov_m32_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0x73F); // 10

    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

/* CP1 computational instructions */

void genabs_s(void)
{
#ifdef INTERPRET_ABS_S
    gencallinterp((unsigned int)cached_interpreter_table.ABS_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fabs_();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void genabs_d(void)
{
#ifdef INTERPRET_ABS_D
    gencallinterp((unsigned int)cached_interpreter_table.ABS_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fabs_();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void genadd_s(void)
{
#ifdef INTERPRET_ADD_S
    gencallinterp((unsigned int)cached_interpreter_table.ADD_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fadd_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void genadd_d(void)
{
#ifdef INTERPRET_ADD_D
    gencallinterp((unsigned int)cached_interpreter_table.ADD_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fadd_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gendiv_s(void)
{
#ifdef INTERPRET_DIV_S
    gencallinterp((unsigned int)cached_interpreter_table.DIV_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fdiv_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gendiv_d(void)
{
#ifdef INTERPRET_DIV_D
    gencallinterp((unsigned int)cached_interpreter_table.DIV_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fdiv_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void genmov_s(void)
{
#ifdef INTERPRET_MOV_S
    gencallinterp((unsigned int)cached_interpreter_table.MOV_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    mov_reg32_preg32(EBX, EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    mov_preg32_reg32(EAX, EBX);
#endif
}

void genmov_d(void)
{
#ifdef INTERPRET_MOV_D
    gencallinterp((unsigned int)cached_interpreter_table.MOV_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    mov_reg32_preg32(EBX, EAX);
    mov_reg32_preg32pimm32(ECX, EAX, 4);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    mov_preg32_reg32(EAX, EBX);
    mov_preg32pimm32_reg32(EAX, 4, ECX);
#endif
}

void genmul_s(void)
{
#ifdef INTERPRET_MUL_S
    gencallinterp((unsigned int)cached_interpreter_table.MUL_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fmul_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void genmul_d(void)
{
#ifdef INTERPRET_MUL_D
    gencallinterp((unsigned int)cached_interpreter_table.MUL_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fmul_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void genneg_s(void)
{
#ifdef INTERPRET_NEG_S
    gencallinterp((unsigned int)cached_interpreter_table.NEG_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fchs();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void genneg_d(void)
{
#ifdef INTERPRET_NEG_D
    gencallinterp((unsigned int)cached_interpreter_table.NEG_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fchs();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gensqrt_s(void)
{
#ifdef INTERPRET_SQRT_S
    gencallinterp((unsigned int)cached_interpreter_table.SQRT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fsqrt();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gensqrt_d(void)
{
#ifdef INTERPRET_SQRT_D
    gencallinterp((unsigned int)cached_interpreter_table.SQRT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fsqrt();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gensub_s(void)
{
#ifdef INTERPRET_SUB_S
    gencallinterp((unsigned int)cached_interpreter_table.SUB_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fsub_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gensub_d(void)
{
#ifdef INTERPRET_SUB_D
    gencallinterp((unsigned int)cached_interpreter_table.SUB_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fsub_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gentrunc_w_s(void)
{
#ifdef INTERPRET_TRUNC_W_S
    gencallinterp((unsigned int)cached_interpreter_table.TRUNC_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&trunc_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_w_d(void)
{
#ifdef INTERPRET_TRUNC_W_D
    gencallinterp((unsigned int)cached_interpreter_table.TRUNC_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&trunc_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_l_s(void)
{
#ifdef INTERPRET_TRUNC_L_S
    gencallinterp((unsigned int)cached_interpreter_table.TRUNC_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&trunc_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_l_d(void)
{
#ifdef INTERPRET_TRUNC_L_D
    gencallinterp((unsigned int)cached_interpreter_table.TRUNC_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&trunc_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_w_s(void)
{
#ifdef INTERPRET_ROUND_W_S
    gencallinterp((unsigned int)cached_interpreter_table.ROUND_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&round_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_w_d(void)
{
#ifdef INTERPRET_ROUND_W_D
    gencallinterp((unsigned int)cached_interpreter_table.ROUND_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&round_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_l_s(void)
{
#ifdef INTERPRET_ROUND_L_S
    gencallinterp((unsigned int)cached_interpreter_table.ROUND_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&round_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_l_d(void)
{
#ifdef INTERPRET_ROUND_L_D
    gencallinterp((unsigned int)cached_interpreter_table.ROUND_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&round_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_w_s(void)
{
#ifdef INTERPRET_CEIL_W_S
    gencallinterp((unsigned int)cached_interpreter_table.CEIL_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&ceil_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_w_d(void)
{
#ifdef INTERPRET_CEIL_W_D
    gencallinterp((unsigned int)cached_interpreter_table.CEIL_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&ceil_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_l_s(void)
{
#ifdef INTERPRET_CEIL_L_S
    gencallinterp((unsigned int)cached_interpreter_table.CEIL_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&ceil_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_l_d(void)
{
#ifdef INTERPRET_CEIL_L_D
    gencallinterp((unsigned int)cached_interpreter_table.CEIL_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&ceil_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_w_s(void)
{
#ifdef INTERPRET_FLOOR_W_S
    gencallinterp((unsigned int)cached_interpreter_table.FLOOR_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&floor_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_w_d(void)
{
#ifdef INTERPRET_FLOOR_W_D
    gencallinterp((unsigned int)cached_interpreter_table.FLOOR_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&floor_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_l_s(void)
{
#ifdef INTERPRET_FLOOR_L_S
    gencallinterp((unsigned int)cached_interpreter_table.FLOOR_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&floor_mode);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_l_d(void)
{
#ifdef INTERPRET_FLOOR_L_D
    gencallinterp((unsigned int)cached_interpreter_table.FLOOR_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16((unsigned short*)&floor_mode);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
    fldcw_m16((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gencvt_s_d(void)
{
#ifdef INTERPRET_CVT_S_D
    gencallinterp((unsigned int)cached_interpreter_table.CVT_S_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gencvt_s_w(void)
{
#ifdef INTERPRET_CVT_S_W
    gencallinterp((unsigned int)cached_interpreter_table.CVT_S_W, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gencvt_s_l(void)
{
#ifdef INTERPRET_CVT_S_L
    gencallinterp((unsigned int)cached_interpreter_table.CVT_S_L, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_dword(EAX);
#endif
}

void gencvt_d_s(void)
{
#ifdef INTERPRET_CVT_D_S
    gencallinterp((unsigned int)cached_interpreter_table.CVT_D_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gencvt_d_w(void)
{
#ifdef INTERPRET_CVT_D_W
    gencallinterp((unsigned int)cached_interpreter_table.CVT_D_W, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gencvt_d_l(void)
{
#ifdef INTERPRET_CVT_D_L
    gencallinterp((unsigned int)cached_interpreter_table.CVT_D_L, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg32_qword(EAX);
#endif
}

void gencvt_w_s(void)
{
#ifdef INTERPRET_CVT_W_S
    gencallinterp((unsigned int)cached_interpreter_table.CVT_W_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
#endif
}

void gencvt_w_d(void)
{
#ifdef INTERPRET_CVT_W_D
    gencallinterp((unsigned int)cached_interpreter_table.CVT_W_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_dword(EAX);
#endif
}

void gencvt_l_s(void)
{
#ifdef INTERPRET_CVT_L_S
    gencallinterp((unsigned int)cached_interpreter_table.CVT_L_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
#endif
}

void gencvt_l_d(void)
{
#ifdef INTERPRET_CVT_L_D
    gencallinterp((unsigned int)cached_interpreter_table.CVT_L_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg32_qword(EAX);
#endif
}

/* CP1 relational instructions */

void genc_f_s(void)
{
#ifdef INTERPRET_C_F_S
    gencallinterp((unsigned int)cached_interpreter_table.C_F_S, 0);
#else
    gencheck_cop1_unusable();
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000);
#endif
}

void genc_f_d(void)
{
#ifdef INTERPRET_C_F_D
    gencallinterp((unsigned int)cached_interpreter_table.C_F_D, 0);
#else
    gencheck_cop1_unusable();
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000);
#endif
}

void genc_un_s(void)
{
#ifdef INTERPRET_C_UN_S
    gencallinterp((unsigned int)cached_interpreter_table.C_UN_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(12);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
    jmp_imm_short(10); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
#endif
}

void genc_un_d(void)
{
#ifdef INTERPRET_C_UN_D
    gencallinterp((unsigned int)cached_interpreter_table.C_UN_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(12);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
    jmp_imm_short(10); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
#endif
}

void genc_eq_s(void)
{
#ifdef INTERPRET_C_EQ_S
    gencallinterp((unsigned int)cached_interpreter_table.C_EQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_eq_d(void)
{
#ifdef INTERPRET_C_EQ_D
    gencallinterp((unsigned int)cached_interpreter_table.C_EQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ueq_s(void)
{
#ifdef INTERPRET_C_UEQ_S
    gencallinterp((unsigned int)cached_interpreter_table.C_UEQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ueq_d(void)
{
#ifdef INTERPRET_C_UEQ_D
    gencallinterp((unsigned int)cached_interpreter_table.C_UEQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_olt_s(void)
{
#ifdef INTERPRET_C_OLT_S
    gencallinterp((unsigned int)cached_interpreter_table.C_OLT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_olt_d(void)
{
#ifdef INTERPRET_C_OLT_D
    gencallinterp((unsigned int)cached_interpreter_table.C_OLT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ult_s(void)
{
#ifdef INTERPRET_C_ULT_S
    gencallinterp((unsigned int)cached_interpreter_table.C_ULT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jae_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ult_d(void)
{
#ifdef INTERPRET_C_ULT_D
    gencallinterp((unsigned int)cached_interpreter_table.C_ULT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jae_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ole_s(void)
{
#ifdef INTERPRET_C_OLE_S
    gencallinterp((unsigned int)cached_interpreter_table.C_OLE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ole_d(void)
{
#ifdef INTERPRET_C_OLE_D
    gencallinterp((unsigned int)cached_interpreter_table.C_OLE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ule_s(void)
{
#ifdef INTERPRET_C_ULE_S
    gencallinterp((unsigned int)cached_interpreter_table.C_ULE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    ja_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ule_d(void)
{
#ifdef INTERPRET_C_ULE_D
    gencallinterp((unsigned int)cached_interpreter_table.C_ULE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    ja_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_sf_s(void)
{
#ifdef INTERPRET_C_SF_S
    gencallinterp((unsigned int)cached_interpreter_table.C_SF_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000);
#endif
}

void genc_sf_d(void)
{
#ifdef INTERPRET_C_SF_D
    gencallinterp((unsigned int)cached_interpreter_table.C_SF_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000);
#endif
}

void genc_ngle_s(void)
{
#ifdef INTERPRET_C_NGLE_S
    gencallinterp((unsigned int)cached_interpreter_table.C_NGLE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(12);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
    jmp_imm_short(10); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
#endif
}

void genc_ngle_d(void)
{
#ifdef INTERPRET_C_NGLE_D
    gencallinterp((unsigned int)cached_interpreter_table.C_NGLE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(12);
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
    jmp_imm_short(10); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
#endif
}

void genc_seq_s(void)
{
#ifdef INTERPRET_C_SEQ_S
    gencallinterp((unsigned int)cached_interpreter_table.C_SEQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_seq_d(void)
{
#ifdef INTERPRET_C_SEQ_D
    gencallinterp((unsigned int)cached_interpreter_table.C_SEQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ngl_s(void)
{
#ifdef INTERPRET_C_NGL_S
    gencallinterp((unsigned int)cached_interpreter_table.C_NGL_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ngl_d(void)
{
#ifdef INTERPRET_C_NGL_D
    gencallinterp((unsigned int)cached_interpreter_table.C_NGL_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jne_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_lt_s(void)
{
#ifdef INTERPRET_C_LT_S
    gencallinterp((unsigned int)cached_interpreter_table.C_LT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_lt_d(void)
{
#ifdef INTERPRET_C_LT_D
    gencallinterp((unsigned int)cached_interpreter_table.C_LT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_nge_s(void)
{
#ifdef INTERPRET_C_NGE_S
    gencallinterp((unsigned int)cached_interpreter_table.C_NGE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jae_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_nge_d(void)
{
#ifdef INTERPRET_C_NGE_D
    gencallinterp((unsigned int)cached_interpreter_table.C_NGE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    jae_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_le_s(void)
{
#ifdef INTERPRET_C_LE_S
    gencallinterp((unsigned int)cached_interpreter_table.C_LE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_le_d(void)
{
#ifdef INTERPRET_C_LE_D
    gencallinterp((unsigned int)cached_interpreter_table.C_LE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ngt_s(void)
{
#ifdef INTERPRET_C_NGT_S
    gencallinterp((unsigned int)cached_interpreter_table.C_NGT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_dword(EAX);
    mov_eax_memoffs32((unsigned int *)(&(r4300_cp1_regs_simple(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_dword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    ja_rj(12);
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}

void genc_ngt_d(void)
{
#ifdef INTERPRET_C_NGT_D
    gencallinterp((unsigned int)cached_interpreter_table.C_NGT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg32_qword(EAX);
    mov_eax_memoffs32((unsigned int*)(&(r4300_cp1_regs_double(&g_dev.r4300.cp1))[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg32_qword(EAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(14);
    ja_rj(12); // 2
    or_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), 0x800000); // 10
    jmp_imm_short(10); // 2
    and_m32_imm32((unsigned int*)&(*r4300_cp1_fcr31(&g_dev.r4300.cp1)), ~0x800000); // 10
#endif
}
