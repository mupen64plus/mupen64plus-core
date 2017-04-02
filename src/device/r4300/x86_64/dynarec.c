/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dynarec.c                                               *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2007 Richard Goedeken (Richard42)                       *
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
#include "assemble_struct.h"
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

#if defined(COUNT_INSTR)
#include "device/r4300/instr_counters.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(offsetof)
#   define offsetof(TYPE,MEMBER) ((unsigned int) &((TYPE*)0)->MEMBER)
#endif


/* These are constants with addresses so that FLDCW can read them.
 * They are declared 'extern' so that other files can do the same. */
const uint16_t trunc_mode = 0xf3f;
const uint16_t round_mode = 0x33f;
const uint16_t ceil_mode  = 0xb3f;
const uint16_t floor_mode = 0x73f;

static const unsigned int precomp_instr_size = sizeof(struct precomp_instr);

/* Dynarec control functions */

void dyna_jump(void)
{
    if (*r4300_stop() == 1)
    {
        dyna_stop();
        return;
    }

    if ((*r4300_pc_struct())->reg_cache_infos.need_map)
    {
        *g_dev.r4300.return_address = (unsigned long long) ((*r4300_pc_struct())->reg_cache_infos.jump_wrapper);
    }
    else
    {
        *g_dev.r4300.return_address = (unsigned long long) (g_dev.r4300.cached_interp.actual->code + (*r4300_pc_struct())->local_addr);
    }
}

void dyna_stop(void)
{
    if (g_dev.r4300.save_rip == 0)
    {
        DebugMessage(M64MSG_WARNING, "Instruction pointer is 0 at dyna_stop()");
    }
    else
    {
        *g_dev.r4300.return_address = (unsigned long long) g_dev.r4300.save_rip;
    }
}


/* M64P Pseudo instructions */

static void gencheck_cop1_unusable(void)
{
    free_registers_move_start();

    test_m32rel_imm32((unsigned int*)&r4300_cp0_regs()[CP0_STATUS_REG], CP0_STATUS_CU1);
    jne_rj(0);
    jump_start_rel8();

    gencallinterp((unsigned long long)dynarec_check_cop1_unusable, 0);

    jump_end_rel8();
}

static void gencp0_update_count(unsigned int addr)
{
#if !defined(COMPARE_CORE) && !defined(DBG)
    mov_reg32_imm32(EAX, addr);
    sub_xreg32_m32rel(EAX, (unsigned int*)(&g_dev.r4300.cp0.last_addr));
    shr_reg32_imm8(EAX, 2);
    mov_xreg32_m32rel(EDX, (void*)&g_dev.r4300.cp0.count_per_op);
    mul_reg32(EDX);
    add_m32rel_xreg32((unsigned int*)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX);
#else
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long)cp0_update_count);
    call_reg64(RAX);
#endif
}

static void gencheck_interrupt(unsigned long long instr_structure)
{
    mov_xreg32_m32rel(EAX, (void*)(r4300_cp0_next_interrupt()));
    cmp_xreg32_m32rel(EAX, (void*)&r4300_cp0_regs()[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(RAX, (unsigned long long) instr_structure);
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void gencheck_interrupt_out(unsigned int addr)
{
    mov_xreg32_m32rel(EAX, (void*)(r4300_cp0_next_interrupt()));
    cmp_xreg32_m32rel(EAX, (void*)&r4300_cp0_regs()[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_m32rel_imm32((unsigned int*)(&g_dev.r4300.fake_instr.addr), addr);
    mov_reg64_imm64(RAX, (unsigned long long) (&g_dev.r4300.fake_instr));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void gencheck_interrupt_reg(void) // addr is in EAX
{
    mov_xreg32_m32rel(EBX, (void*)r4300_cp0_next_interrupt());
    cmp_xreg32_m32rel(EBX, (void*)&r4300_cp0_regs()[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_m32rel_xreg32((unsigned int*)(&g_dev.r4300.fake_instr.addr), EAX);
    mov_reg64_imm64(RAX, (unsigned long long) (&g_dev.r4300.fake_instr));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void ld_register_alloc(int *pGpr1, int *pGpr2, int *pBase1, int *pBase2)
{
    int gpr1, gpr2, base1, base2 = 0;

#ifdef COMPARE_CORE
    free_registers_move_start(); // to maintain parity with 32-bit core
#endif

    if (g_dev.r4300.recomp.dst->f.i.rs == g_dev.r4300.recomp.dst->f.i.rt)
    {
        allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);          // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        gpr2 = lock_register(lru_register());                      // free and lock least recently used register for usage here
        add_reg32_imm32(gpr1, (int)g_dev.r4300.recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr2, gpr1);
    }
    else
    {
        gpr2 = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);   // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        free_register(gpr2);                                       // write out gpr2 if dirty because I'm going to trash it right now
        add_reg32_imm32(gpr2, (int)g_dev.r4300.recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr1, gpr2);
        lock_register(gpr2);                                       // lock the freed gpr2 it so it doesn't get returned in the lru query
    }
    base1 = lock_register(lru_base_register());                  // get another lru register
    if (!g_dev.r4300.recomp.fast_memory)
    {
        base2 = lock_register(lru_base_register());                // and another one if necessary
        unlock_register(base2);
    }
    unlock_register(base1);                                      // unlock the locked registers (they are
    unlock_register(gpr2);
    set_register_state(gpr1, NULL, 0, 0);                        // clear gpr1 state because it hasn't been written yet -
    // we don't want it to be pushed/popped around read_rdramX call
    *pGpr1 = gpr1;
    *pGpr2 = gpr2;
    *pBase1 = base1;
    *pBase2 = base2;
}

#ifdef COMPARE_CORE
extern unsigned int op; /* api/debugger.c */

void gendebug(void)
{
    free_all_registers();

    mov_memoffs64_rax((unsigned long long *) &g_dev.r4300.debug_reg_storage);
    mov_reg64_imm64(RAX, (unsigned long long) &g_dev.r4300.debug_reg_storage);
    mov_preg64pimm8_reg64(RAX,  8, RBX);
    mov_preg64pimm8_reg64(RAX, 16, RCX);
    mov_preg64pimm8_reg64(RAX, 24, RDX);
    mov_preg64pimm8_reg64(RAX, 32, RSP);
    mov_preg64pimm8_reg64(RAX, 40, RBP);
    mov_preg64pimm8_reg64(RAX, 48, RSI);
    mov_preg64pimm8_reg64(RAX, 56, RDI);

    mov_reg64_imm64(RAX, (unsigned long long) g_dev.r4300.recomp.dst);
    mov_memoffs64_rax((unsigned long long *) &(*r4300_pc_struct()));
    mov_reg32_imm32(EAX, (unsigned int) g_dev.r4300.recomp.src);
    mov_memoffs32_eax((unsigned int *) &op);
    mov_reg64_imm64(RAX, (unsigned long long) CoreCompareCallback);
    call_reg64(RAX);

    mov_reg64_imm64(RAX, (unsigned long long) &g_dev.r4300.debug_reg_storage);
    mov_reg64_preg64pimm8(RDI, RAX, 56);
    mov_reg64_preg64pimm8(RSI, RAX, 48);
    mov_reg64_preg64pimm8(RBP, RAX, 40);
    mov_reg64_preg64pimm8(RSP, RAX, 32);
    mov_reg64_preg64pimm8(RDX, RAX, 24);
    mov_reg64_preg64pimm8(RCX, RAX, 16);
    mov_reg64_preg64pimm8(RBX, RAX,  8);
    mov_reg64_preg64(RAX, RAX);
}
#endif

void genni(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[1]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.NI, 0);
}

void gennotcompiled(void)
{
    free_registers_move_start();

    mov_reg64_imm64(RAX, (unsigned long long) g_dev.r4300.recomp.dst);
    mov_memoffs64_rax((unsigned long long *) &(*r4300_pc_struct())); /* RIP-relative will not work here */
    mov_reg64_imm64(RAX, (unsigned long long) cached_interpreter_table.NOTCOMPILED);
    call_reg64(RAX);
}

void genlink_subblock(void)
{
    free_all_registers();
    jmp(g_dev.r4300.recomp.dst->addr+4);
}

void genfin_block(void)
{
    gencallinterp((unsigned long long)cached_interpreter_table.FIN_BLOCK, 0);
}

void gencallinterp(uintptr_t addr, int jump)
{
    free_registers_move_start();

    if (jump) {
        mov_m32rel_imm32((unsigned int*)(&g_dev.r4300.dyna_interp), 1);
    }

    mov_reg64_imm64(RAX, (unsigned long long) g_dev.r4300.recomp.dst);
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, addr);
    call_reg64(RAX);

    if (jump)
    {
        mov_m32rel_imm32((unsigned int*)(&g_dev.r4300.dyna_interp), 0);
        mov_reg64_imm64(RAX, (unsigned long long)dyna_jump);
        call_reg64(RAX);
    }
}

void gendelayslot(void)
{
    mov_m32rel_imm32((void*)(&g_dev.r4300.delay_slot), 1);
    recompile_opcode(&g_dev.r4300);

    free_all_registers();
    gencp0_update_count(g_dev.r4300.recomp.dst->addr+4);

    mov_m32rel_imm32((void*)(&g_dev.r4300.delay_slot), 0);
}

/* Reserved */

void genreserved(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[0]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.RESERVED, 0);
}

/* Load instructions */

void genlb(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[24]);
#endif
#ifdef INTERPRET_LB
    gencallinterp((unsigned long long)cached_interpreter_table.LB, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmemb);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdramb);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr2, gpr2, base1);
    call_reg64(gpr2);
    movsx_xreg32_m8rel(gpr1, (unsigned char *)g_dev.r4300.recomp.dst->f.i.rt);
    jmp_imm_short(24);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 3); // 4
    movsx_reg32_8preg64preg64(gpr1, gpr2, base1); // 4

    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlbu(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[28]);
#endif
#ifdef INTERPRET_LBU
    gencallinterp((unsigned long long)cached_interpreter_table.LBU, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmemb);
    if(g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdramb);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr2, gpr2, base1);
    call_reg64(gpr2);
    mov_xreg32_m32rel(gpr1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    jmp_imm_short(23);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 3); // 4
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    and_reg32_imm32(gpr1, 0xFF);
    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlh(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[25]);
#endif
#ifdef INTERPRET_LH
    gencallinterp((unsigned long long)cached_interpreter_table.LH, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmemh);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdramh);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr2, gpr2, base1);
    call_reg64(gpr2);
    movsx_xreg32_m16rel(gpr1, (unsigned short *)g_dev.r4300.recomp.dst->f.i.rt);
    jmp_imm_short(24);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 2); // 4
    movsx_reg32_16preg64preg64(gpr1, gpr2, base1); // 4

    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlhu(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[29]);
#endif
#ifdef INTERPRET_LHU
    gencallinterp((unsigned long long)cached_interpreter_table.LHU, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmemh);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdramh);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr2, gpr2, base1);
    call_reg64(gpr2);
    mov_xreg32_m32rel(gpr1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    jmp_imm_short(23);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 2); // 4
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    and_reg32_imm32(gpr1, 0xFFFF);
    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 0);
#endif
}

void genll(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[42]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.LL, 0);
}

void genlw(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[26]);
#endif
#ifdef INTERPRET_LW
    gencallinterp((unsigned long long)cached_interpreter_table.LW, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmem);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdram);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    jne_rj(21);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3
    jmp_imm_short(0); // 2
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr1, gpr2, base1);
    call_reg64(gpr1);
    mov_xreg32_m32rel(gpr1, (unsigned int *)(g_dev.r4300.recomp.dst->f.i.rt));

    jump_end_rel8();

    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 0);     // set gpr1 state as dirty, and bound to r4300 reg RT
#endif
}

void genlwu(void)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[30]);
#endif
#ifdef INTERPRET_LWU
    gencallinterp((unsigned long long)cached_interpreter_table.LWU, 0);
#else
    free_registers_move_start();

    ld_register_alloc(&gpr1, &gpr2, &base1, &base2);

    mov_reg64_imm64(base1, (unsigned long long) g_dev.mem.readmem);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_reg32_imm32(gpr1, 0xDF800000);
        cmp_reg32_imm32(gpr1, 0x80000000);
    }
    else
    {
        mov_reg64_imm64(base2, (unsigned long long) read_rdram);
        shr_reg32_imm8(gpr1, 16);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        cmp_reg64_reg64(gpr1, base2);
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), gpr1);
    shr_reg32_imm8(gpr2, 16);
    mov_reg64_preg64x8preg64(gpr2, gpr2, base1);
    call_reg64(gpr2);
    mov_xreg32_m32rel(gpr1, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    jmp_imm_short(19);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    set_register_state(gpr1, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 1);
#endif
}

void genlwl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[27]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.LWL, 0);
}

void genlwr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[31]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.LWR, 0);
}

void genld(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[41]);
#endif
#ifdef INTERPRET_LD
    gencallinterp((unsigned long long)cached_interpreter_table.LD, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.readmemd);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) read_rdramd);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(59);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_reg64_imm64(RAX, (unsigned long long) g_dev.r4300.recomp.dst->f.i.rt); // 10
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), RAX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg64_m64rel(RAX, (unsigned long long *)(g_dev.r4300.recomp.dst->f.i.rt)); // 7
    jmp_imm_short(33); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6

    mov_reg32_preg64preg64(EAX, RBX, RSI); // 3
    mov_reg32_preg64preg64pimm32(EBX, RBX, RSI, 4); // 7
    shl_reg64_imm8(RAX, 32); // 4
    or_reg64_reg64(RAX, RBX); // 3

    set_register_state(RAX, (unsigned int*)g_dev.r4300.recomp.dst->f.i.rt, 1, 1);
#endif
}

void genldl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[22]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.LDL, 0);
}

void genldr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[23]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.LDR, 0);
}

/* Store instructions */

void gensb(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[32]);
#endif
#ifdef INTERPRET_SB
    gencallinterp((unsigned long long)cached_interpreter_table.SB, 0);
#else
    free_registers_move_start();

    mov_xreg8_m8rel(CL, (unsigned char *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writememb);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdramb);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(49);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m8rel_xreg8((unsigned char *)(r4300_wbyte()), CL); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(25); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 3); // 4
    mov_preg64preg64_reg8(RBX, RSI, CL); // 3

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void gensh(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[33]);
#endif
#ifdef INTERPRET_SH
    gencallinterp((unsigned long long)cached_interpreter_table.SH, 0);
#else
    free_registers_move_start();

    mov_xreg16_m16rel(CX, (unsigned short *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writememh);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdramh);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(50);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m16rel_xreg16((unsigned short *)(r4300_whword()), CX); // 8
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(26); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 2); // 4
    mov_preg64preg64_reg16(RBX, RSI, CX); // 4

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void gensc(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[46]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.SC, 0);
}

void gensw(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[34]);
#endif
#ifdef INTERPRET_SW
    gencallinterp((unsigned long long)cached_interpreter_table.SW, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writemem);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdram);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(49);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wword()), ECX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(21); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64_reg32(RBX, RSI, ECX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void genswl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[35]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.SWL, 0);
}

void genswr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[36]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.SWR, 0);
}

void gensd(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[45]);
#endif
#ifdef INTERPRET_SD
    gencallinterp((unsigned long long)cached_interpreter_table.SD, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EDX, ((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt)+1);
    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writememd);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdramd);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(56);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword()), ECX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword())+1, EDX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64pimm32_reg32(RBX, RSI, 4, ECX); // 7
    mov_preg64preg64_reg32(RBX, RSI, EDX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void gensdl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[37]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.SDL, 0);
}

void gensdr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[38]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.SDR, 0);
}

/* Computational instructions */

void genadd(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[79]);
#endif
#ifdef INTERPRET_ADD
    gencallinterp((unsigned long long)cached_interpreter_table.ADD, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        add_reg32_reg32(rd, rt);
    }
    else if (rt == rd) {
        add_reg32_reg32(rd, rs);
    }
    else
    {
        mov_reg32_reg32(rd, rs);
        add_reg32_reg32(rd, rt);
    }
#endif
}

void genaddu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[80]);
#endif
#ifdef INTERPRET_ADDU
    gencallinterp((unsigned long long)cached_interpreter_table.ADDU, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        add_reg32_reg32(rd, rt);
    }
    else if (rt == rd) {
        add_reg32_reg32(rd, rs);
    }
    else
    {
        mov_reg32_reg32(rd, rs);
        add_reg32_reg32(rd, rt);
    }
#endif
}

void genaddi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[8]);
#endif
#ifdef INTERPRET_ADDI
    gencallinterp((unsigned long long)cached_interpreter_table.ADDI, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genaddiu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[9]);
#endif
#ifdef INTERPRET_ADDIU
    gencallinterp((unsigned long long)cached_interpreter_table.ADDIU, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gendadd(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[89]);
#endif
#ifdef INTERPRET_DADD
    gencallinterp((unsigned long long)cached_interpreter_table.DADD, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        add_reg64_reg64(rd, rt);
    }
    else if (rt == rd) {
        add_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        add_reg64_reg64(rd, rt);
    }
#endif
}

void gendaddu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[90]);
#endif
#ifdef INTERPRET_DADDU
    gencallinterp((unsigned long long)cached_interpreter_table.DADDU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        add_reg64_reg64(rd, rt);
    }
    else if (rt == rd) {
        add_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        add_reg64_reg64(rd, rt);
    }
#endif
}

void gendaddi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[20]);
#endif
#ifdef INTERPRET_DADDI
    gencallinterp((unsigned long long)cached_interpreter_table.DADDI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    add_reg64_imm32(rt, (int) g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gendaddiu(void)
{
#ifdef INTERPRET_DADDIU
    gencallinterp((unsigned long long)cached_interpreter_table.DADDIU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    add_reg64_imm32(rt, (int) g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gensub(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[81]);
#endif
#ifdef INTERPRET_SUB
    gencallinterp((unsigned long long)cached_interpreter_table.SUB, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        sub_reg32_reg32(rd, rt);
    }
    else if (rt == rd)
    {
        neg_reg32(rd);
        add_reg32_reg32(rd, rs);
    }
    else
    {
        mov_reg32_reg32(rd, rs);
        sub_reg32_reg32(rd, rt);
    }
#endif
}

void gensubu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[82]);
#endif
#ifdef INTERPRET_SUBU
    gencallinterp((unsigned long long)cached_interpreter_table.SUBU, 0);
#else
    int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        sub_reg32_reg32(rd, rt);
    }
    else if (rt == rd)
    {
        neg_reg32(rd);
        add_reg32_reg32(rd, rs);
    }
    else
    {
        mov_reg32_reg32(rd, rs);
        sub_reg32_reg32(rd, rt);
    }
#endif
}

void gendsub(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[91]);
#endif
#ifdef INTERPRET_DSUB
    gencallinterp((unsigned long long)cached_interpreter_table.DSUB, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        sub_reg64_reg64(rd, rt);
    }
    else if (rt == rd)
    {
        neg_reg64(rd);
        add_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        sub_reg64_reg64(rd, rt);
    }
#endif
}

void gendsubu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[92]);
#endif
#ifdef INTERPRET_DSUBU
    gencallinterp((unsigned long long)cached_interpreter_table.DSUBU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        sub_reg64_reg64(rd, rt);
    }
    else if (rt == rd)
    {
        neg_reg64(rd);
        add_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        sub_reg64_reg64(rd, rt);
    }
#endif
}

void genslt(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[87]);
#endif
#ifdef INTERPRET_SLT
    gencallinterp((unsigned long long)cached_interpreter_table.SLT, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    cmp_reg64_reg64(rs, rt);
    setl_reg8(rd);
    and_reg64_imm8(rd, 1);
#endif
}

void gensltu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[88]);
#endif
#ifdef INTERPRET_SLTU
    gencallinterp((unsigned long long)cached_interpreter_table.SLTU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    cmp_reg64_reg64(rs, rt);
    setb_reg8(rd);
    and_reg64_imm8(rd, 1);
#endif
}

void genslti(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[10]);
#endif
#ifdef INTERPRET_SLTI
    gencallinterp((unsigned long long)cached_interpreter_table.SLTI, 0);
#else
    int rs = allocate_register_64((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rt);
    int imm = (int) g_dev.r4300.recomp.dst->f.i.immediate;

    cmp_reg64_imm32(rs, imm);
    setl_reg8(rt);
    and_reg64_imm8(rt, 1);
#endif
}

void gensltiu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[11]);
#endif
#ifdef INTERPRET_SLTIU
    gencallinterp((unsigned long long)cached_interpreter_table.SLTIU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);
    int imm = (int) g_dev.r4300.recomp.dst->f.i.immediate;

    cmp_reg64_imm32(rs, imm);
    setb_reg8(rt);
    and_reg64_imm8(rt, 1);
#endif
}

void genand(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[83]);
#endif
#ifdef INTERPRET_AND
    gencallinterp((unsigned long long)cached_interpreter_table.AND, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        and_reg64_reg64(rd, rt);
    }
    else if (rt == rd) {
        and_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        and_reg64_reg64(rd, rt);
    }
#endif
}

void genandi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[12]);
#endif
#ifdef INTERPRET_ANDI
    gencallinterp((unsigned long long)cached_interpreter_table.ANDI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    and_reg64_imm32(rt, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genor(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[84]);
#endif
#ifdef INTERPRET_OR
    gencallinterp((unsigned long long)cached_interpreter_table.OR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        or_reg64_reg64(rd, rt);
    }
    else if (rt == rd) {
        or_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        or_reg64_reg64(rd, rt);
    }
#endif
}

void genori(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[13]);
#endif
#ifdef INTERPRET_ORI
    gencallinterp((unsigned long long)cached_interpreter_table.ORI, 0);
#else
    int rs = allocate_register_64((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    or_reg64_imm32(rt, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void genxor(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[85]);
#endif
#ifdef INTERPRET_XOR
    gencallinterp((unsigned long long)cached_interpreter_table.XOR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd) {
        xor_reg64_reg64(rd, rt);
    }
    else if (rt == rd) {
        xor_reg64_reg64(rd, rs);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        xor_reg64_reg64(rd, rt);
    }
#endif
}

void genxori(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[14]);
#endif
#ifdef INTERPRET_XORI
    gencallinterp((unsigned long long)cached_interpreter_table.XORI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    xor_reg64_imm32(rt, (unsigned short)g_dev.r4300.recomp.dst->f.i.immediate);
#endif
}

void gennor(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[86]);
#endif
#ifdef INTERPRET_NOR
    gencallinterp((unsigned long long)cached_interpreter_table.NOR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rs == rd)
    {
        or_reg64_reg64(rd, rt);
        not_reg64(rd);
    }
    else if (rt == rd)
    {
        or_reg64_reg64(rd, rs);
        not_reg64(rd);
    }
    else
    {
        mov_reg64_reg64(rd, rs);
        or_reg64_reg64(rd, rt);
        not_reg64(rd);
    }
#endif
}

void genlui(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[15]);
#endif
#ifdef INTERPRET_LUI
    gencallinterp((unsigned long long)cached_interpreter_table.LUI, 0);
#else
    int rt = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    mov_reg32_imm32(rt, (unsigned int)g_dev.r4300.recomp.dst->f.i.immediate << 16);
#endif
}

/* Shift instructions */

void gennop(void)
{
}

void gensll(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[55]);
#endif
#ifdef INTERPRET_SLL
    gencallinterp((unsigned long long)cached_interpreter_table.SLL, 0);
#else
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shl_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensllv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[58]);
#endif
#ifdef INTERPRET_SLLV
    gencallinterp((unsigned long long)cached_interpreter_table.SLLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[93]);
#endif
#ifdef INTERPRET_DSLL
    gencallinterp((unsigned long long)cached_interpreter_table.DSLL, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shl_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gendsllv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[68]);
#endif
#ifdef INTERPRET_DSLLV
    gencallinterp((unsigned long long)cached_interpreter_table.DSLLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg64_reg64(rd, rt);
        shl_reg64_cl(rd);
    }
    else
    {
        int temp;
        temp = lru_register();
        free_register(temp);

        mov_reg64_reg64(temp, rt);
        shl_reg64_cl(temp);
        mov_reg64_reg64(rd, temp);
    }
#endif
}

void gendsll32(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[97]);
#endif
#ifdef INTERPRET_DSLL32
    gencallinterp((unsigned long long)cached_interpreter_table.DSLL32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shl_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa + 32);
#endif
}

void gensrl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[56]);
#endif
#ifdef INTERPRET_SRL
    gencallinterp((unsigned long long)cached_interpreter_table.SRL, 0);
#else
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shr_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensrlv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[59]);
#endif
#ifdef INTERPRET_SRLV
    gencallinterp((unsigned long long)cached_interpreter_table.SRLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[94]);
#endif
#ifdef INTERPRET_DSRL
    gencallinterp((unsigned long long)cached_interpreter_table.DSRL, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shr_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gendsrlv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[69]);
#endif
#ifdef INTERPRET_DSRLV
    gencallinterp((unsigned long long)cached_interpreter_table.DSRLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg64_reg64(rd, rt);
        shr_reg64_cl(rd);
    }
    else
    {
        int temp;
        temp = lru_register();
        free_register(temp);

        mov_reg64_reg64(temp, rt);
        shr_reg64_cl(temp);
        mov_reg64_reg64(rd, temp);
    }
#endif
}

void gendsrl32(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[98]);
#endif
#ifdef INTERPRET_DSRL32
    gencallinterp((unsigned long long)cached_interpreter_table.DSRL32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shr_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa + 32);
#endif
}

void gensra(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[57]);
#endif
#ifdef INTERPRET_SRA
    gencallinterp((unsigned long long)cached_interpreter_table.SRA, 0);
#else
    int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    sar_reg32_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gensrav(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[60]);
#endif
#ifdef INTERPRET_SRAV
    gencallinterp((unsigned long long)cached_interpreter_table.SRAV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)g_dev.r4300.recomp.dst->f.r.rd);

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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[95]);
#endif
#ifdef INTERPRET_DSRA
    gencallinterp((unsigned long long)cached_interpreter_table.DSRA, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    sar_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa);
#endif
}

void gendsrav(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[70]);
#endif
#ifdef INTERPRET_DSRAV
    gencallinterp((unsigned long long)cached_interpreter_table.DSRAV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    if (rd != ECX)
    {
        mov_reg64_reg64(rd, rt);
        sar_reg64_cl(rd);
    }
    else
    {
        int temp;
        temp = lru_register();
        free_register(temp);

        mov_reg64_reg64(temp, rt);
        sar_reg64_cl(temp);
        mov_reg64_reg64(rd, temp);
    }
#endif
}

void gendsra32(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[99]);
#endif
#ifdef INTERPRET_DSRA32
    gencallinterp((unsigned long long)cached_interpreter_table.DSRA32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    sar_reg64_imm8(rd, g_dev.r4300.recomp.dst->f.r.sa + 32);
#endif
}

/* Multiply / Divide instructions */

void genmult(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[71]);
#endif
#ifdef INTERPRET_MULT
    gencallinterp((unsigned long long)cached_interpreter_table.MULT, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo()); /* these must be done first so they are not assigned by allocate_register() */
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi());
    rs = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    imul_reg32(rt);
#endif
}

void genmultu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[72]);
#endif
#ifdef INTERPRET_MULTU
    gencallinterp((unsigned long long)cached_interpreter_table.MULTU, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo());
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi());
    rs = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    mul_reg32(rt);
#endif
}

void gendmult(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[75]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.DMULT, 0);
}

void gendmultu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[76]);
#endif
#ifdef INTERPRET_DMULTU
    gencallinterp((unsigned long long)cached_interpreter_table.DMULTU, 0);
#else
    free_registers_move_start();

    mov_xreg64_m64rel(RAX, (unsigned long long *) g_dev.r4300.recomp.dst->f.r.rs);
    mov_xreg64_m64rel(RDX, (unsigned long long *) g_dev.r4300.recomp.dst->f.r.rt);
    mul_reg64(RDX);
    mov_m64rel_xreg64((unsigned long long *) r4300_mult_lo(), RAX);
    mov_m64rel_xreg64((unsigned long long *) r4300_mult_hi(), RDX);
#endif
}

void gendiv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[73]);
#endif
#ifdef INTERPRET_DIV
    gencallinterp((unsigned long long)cached_interpreter_table.DIV, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo());
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi());
    rs = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 1 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    cdq(); // 1
    idiv_reg32(rt); // 2
#endif
}

void gendivu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[74]);
#endif
#ifdef INTERPRET_DIVU
    gencallinterp((unsigned long long)cached_interpreter_table.DIVU, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo());
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi());
    rs = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 2 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    xor_reg32_reg32(EDX, EDX); // 2
    div_reg32(rt); // 2
#endif
}

void genddiv(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[77]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.DDIV, 0);
}

void genddivu(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[78]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.DDIVU, 0);
}

void genmfhi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[64]);
#endif
#ifdef INTERPRET_MFHI
    gencallinterp((unsigned long long)cached_interpreter_table.MFHI, 0);
#else
    int rd = allocate_register_64_w((unsigned long long *) g_dev.r4300.recomp.dst->f.r.rd);
    int _hi = allocate_register_64((unsigned long long *) r4300_mult_hi());

    mov_reg64_reg64(rd, _hi);
#endif
}

void genmthi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[65]);
#endif
#ifdef INTERPRET_MTHI
    gencallinterp((unsigned long long)cached_interpreter_table.MTHI, 0);
#else
    int _hi = allocate_register_64_w((unsigned long long *) r4300_mult_hi());
    int rs = allocate_register_64((unsigned long long *) g_dev.r4300.recomp.dst->f.r.rs);

    mov_reg64_reg64(_hi, rs);
#endif
}

void genmflo(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[66]);
#endif
#ifdef INTERPRET_MFLO
    gencallinterp((unsigned long long)cached_interpreter_table.MFLO, 0);
#else
    int rd = allocate_register_64_w((unsigned long long *) g_dev.r4300.recomp.dst->f.r.rd);
    int _lo = allocate_register_64((unsigned long long *) r4300_mult_lo());

    mov_reg64_reg64(rd, _lo);
#endif
}

void genmtlo(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[67]);
#endif
#ifdef INTERPRET_MTLO
    gencallinterp((unsigned long long)cached_interpreter_table.MTLO, 0);
#else
    int _lo = allocate_register_64_w((unsigned long long *)r4300_mult_lo());
    int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.r.rs);

    mov_reg64_reg64(_lo, rs);
#endif
}

/* Jump & Branch instructions */

static void gentest(void)
{
    cmp_m32rel_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt((unsigned long long) (g_dev.r4300.recomp.dst + (g_dev.r4300.recomp.dst-1)->f.i.immediate));
    jmp(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned long long)(g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentest_out(void)
{
    cmp_m32rel_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32rel_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);
    jump_end_rel32();

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned long long) (g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentest_idle(void)
{
    int reg;

    reg = lru_register();
    free_register(reg);

    cmp_m32rel_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_xreg32_m32rel(reg, (unsigned int *)(r4300_cp0_next_interrupt()));
    sub_xreg32_m32rel(reg, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
    cmp_reg32_imm8(reg, 3);
    jbe_rj(0);
    jump_start_rel8();

    and_reg32_imm32(reg, 0xFFFFFFFC);
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), reg);

    jump_end_rel8();
    jump_end_rel32();
}

static void gentestl(void)
{
    cmp_m32rel_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    gendelayslot();
    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt((unsigned long long) (g_dev.r4300.recomp.dst + (g_dev.r4300.recomp.dst-1)->f.i.immediate));
    jmp(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    gencp0_update_count(g_dev.r4300.recomp.dst->addr-4);
    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned long long) (g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void gentestl_out(void)
{
    cmp_m32rel_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    gendelayslot();
    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
    mov_m32rel_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);

    jump_end_rel32();

    gencp0_update_count(g_dev.r4300.recomp.dst->addr-4);
    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), g_dev.r4300.recomp.dst->addr + 4);
    gencheck_interrupt((unsigned long long) (g_dev.r4300.recomp.dst + 1));
    jmp(g_dev.r4300.recomp.dst->addr + 4);
}

static void genbranchlink(void)
{
    int r31_64bit = is64((unsigned int*)&r4300_regs()[31]);

    if (r31_64bit == 0)
    {
        int r31 = allocate_register_32_w((unsigned int *)&r4300_regs()[31]);

        mov_reg32_imm32(r31, g_dev.r4300.recomp.dst->addr+8);
    }
    else if (r31_64bit == -1)
    {
        mov_m32rel_imm32((unsigned int *)&r4300_regs()[31], g_dev.r4300.recomp.dst->addr + 8);
        if (g_dev.r4300.recomp.dst->addr & 0x80000000) {
            mov_m32rel_imm32(((unsigned int *)&r4300_regs()[31])+1, 0xFFFFFFFF);
        }
        else {
            mov_m32rel_imm32(((unsigned int *)&r4300_regs()[31])+1, 0);
        }
    }
    else
    {
        int r31 = allocate_register_64_w((unsigned long long *)&r4300_regs()[31]);

        mov_reg32_imm32(r31, g_dev.r4300.recomp.dst->addr+8);
        movsxd_reg64_reg32(r31, r31);
    }
}

void genj(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J
    gencallinterp((unsigned long long)cached_interpreter_table.J, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.J, 1);
        return;
    }

    gendelayslot();
    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), naddr);
    gencheck_interrupt((unsigned long long) &g_dev.r4300.cached_interp.actual->block[(naddr-g_dev.r4300.cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genj_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.J_OUT, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.J_OUT, 1);
        return;
    }

    gendelayslot();
    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), naddr);
    gencheck_interrupt_out(naddr);
    mov_m32rel_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long)dynarec_jump_to_address);
    call_reg64(RAX);
#endif
}

void genj_idle(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J_IDLE
    gencallinterp((unsigned long long)cached_interpreter_table.J_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.J_IDLE, 1);
        return;
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_cp0_next_interrupt()));
    sub_xreg32_m32rel(EAX, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(12);

    and_eax_imm32(0xFFFFFFFC);  // 5
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX); // 7

    genj();
#endif
}

void genjal(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL
    gencallinterp((unsigned long long)cached_interpreter_table.JAL, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.JAL, 1);
        return;
    }

    gendelayslot();

    mov_m32rel_imm32((unsigned int *)(r4300_regs() + 31), g_dev.r4300.recomp.dst->addr + 4);
    if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs()[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs()[31])+1, 0);
    }

    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), naddr);
    gencheck_interrupt((unsigned long long) &g_dev.r4300.cached_interp.actual->block[(naddr-g_dev.r4300.cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genjal_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.JAL_OUT, 1);
#else
    unsigned int naddr;

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.JAL_OUT, 1);
        return;
    }

    gendelayslot();

    mov_m32rel_imm32((unsigned int *)(r4300_regs() + 31), g_dev.r4300.recomp.dst->addr + 4);
    if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs()[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs()[31])+1, 0);
    }

    naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&g_dev.r4300.cp0.last_addr), naddr);
    gencheck_interrupt_out(naddr);
    mov_m32rel_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);
#endif
}

void genjal_idle(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL_IDLE
    gencallinterp((unsigned long long)cached_interpreter_table.JAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.JAL_IDLE, 1);
        return;
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_cp0_next_interrupt()));
    sub_xreg32_m32rel(EAX, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(12);

    and_eax_imm32(0xFFFFFFFC);  // 5
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX); // 7

    genjal();
#endif
}

void genjr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[61]);
#endif
#ifdef INTERPRET_JR
    gencallinterp((unsigned long long)cached_interpreter_table.JR, 1);
#else
    unsigned int diff = (unsigned int) offsetof(struct precomp_instr, local_addr);
    unsigned int diff_need = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.need_map);
    unsigned int diff_wrap = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.jump_wrapper);

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.JR, 1);
        return;
    }

    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    mov_m32rel_xreg32((unsigned int *)&g_dev.r4300.local_rs, EAX);

    gendelayslot();

    mov_xreg32_m32rel(EAX, (unsigned int *)&g_dev.r4300.local_rs);
    mov_m32rel_xreg32((unsigned int *)&g_dev.r4300.cp0.last_addr, EAX);

    gencheck_interrupt_reg();

    mov_xreg32_m32rel(EAX, (unsigned int *)&g_dev.r4300.local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(g_dev.r4300.recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32rel_xreg32(&g_dev.r4300.recomp.jump_to_address, EBX);
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);  /* will never return from call */

    jump_end_rel32();

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.recomp.dst_block->block);
    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(g_dev.r4300.recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32rel((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(11);

    add_reg32_imm32(EAX, diff_wrap); // 6
    add_reg64_reg64(RAX, RSI); // 3
    jmp_reg64(RAX); // 2

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff);
    mov_rax_memoffs64((unsigned long long *) &g_dev.r4300.recomp.dst_block->code);
    add_reg64_reg64(RAX, RBX);
    jmp_reg64(RAX);
#endif
}

void genjalr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[62]);
#endif
#ifdef INTERPRET_JALR
    gencallinterp((unsigned long long)cached_interpreter_table.JALR, 0);
#else
    unsigned int diff = (unsigned int) offsetof(struct precomp_instr, local_addr);
    unsigned int diff_need = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.need_map);
    unsigned int diff_wrap = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.jump_wrapper);

    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.JALR, 1);
        return;
    }

    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)g_dev.r4300.recomp.dst->f.r.rs);
    mov_m32rel_xreg32((unsigned int *)&g_dev.r4300.local_rs, EAX);

    gendelayslot();

    mov_m32rel_imm32((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd, g_dev.r4300.recomp.dst->addr+4);
    if ((g_dev.r4300.recomp.dst->addr+4) & 0x80000000) {
        mov_m32rel_imm32(((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd)+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32(((unsigned int *)(g_dev.r4300.recomp.dst-1)->f.r.rd)+1, 0);
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)&g_dev.r4300.local_rs);
    mov_m32rel_xreg32((unsigned int *)&g_dev.r4300.cp0.last_addr, EAX);

    gencheck_interrupt_reg();

    mov_xreg32_m32rel(EAX, (unsigned int *)&g_dev.r4300.local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(g_dev.r4300.recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32rel_xreg32(&g_dev.r4300.recomp.jump_to_address, EBX);
    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);  /* will never return from call */

    jump_end_rel32();

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.recomp.dst_block->block);
    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(g_dev.r4300.recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32rel((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(11);

    add_reg32_imm32(EAX, diff_wrap); // 6
    add_reg64_reg64(RAX, RSI); // 3
    jmp_reg64(RAX); // 2

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff);
    mov_rax_memoffs64((unsigned long long *) &g_dev.r4300.recomp.dst_block->code);
    add_reg64_reg64(RAX, RBX);
    jmp_reg64(RAX);
#endif
}

static void genbeq_test(void)
{
    int rs_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
    int rt_64bit = is64((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

    if (rs_64bit == 0 && rt_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        sete_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rs_64bit == -1)
    {
        int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_xreg64_m64rel(rt, (unsigned long long *) g_dev.r4300.recomp.dst->f.i.rs);
        sete_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rt_64bit == -1)
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_xreg64_m64rel(rs, (unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);
        sete_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);
        cmp_reg64_reg64(rs, rt);
        sete_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genbeq(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[4]);
#endif
#ifdef INTERPRET_BEQ
    gencallinterp((unsigned long long)cached_interpreter_table.BEQ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQ, 1);
        return;
    }

    genbeq_test();
    gendelayslot();
    gentest();
#endif
}

void genbeq_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[4]);
#endif
#ifdef INTERPRET_BEQ_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BEQ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQ_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BEQ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQ_IDLE, 1);
        return;
    }

    genbeq_test();
    gentest_idle();
    genbeq();
#endif
}

void genbeql(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[16]);
#endif
#ifdef INTERPRET_BEQL
    gencallinterp((unsigned long long)cached_interpreter_table.BEQL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQL, 1);
        return;
    }

    genbeq_test();
    free_all_registers();
    gentestl();
#endif
}

void genbeql_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[16]);
#endif
#ifdef INTERPRET_BEQL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BEQL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BEQL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BEQL_IDLE, 1);
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

    if (rs_64bit == 0 && rt_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        setne_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rs_64bit == -1)
    {
        int rt = allocate_register_64((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rt);

        cmp_xreg64_m64rel(rt, (unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
        setne_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rt_64bit == -1)
    {
        int rs = allocate_register_64((unsigned long long *) g_dev.r4300.recomp.dst->f.i.rs);

        cmp_xreg64_m64rel(rs, (unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);
        setne_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
        int rt = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rt);

        cmp_reg64_reg64(rs, rt);
        setne_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genbne(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[5]);
#endif
#ifdef INTERPRET_BNE
    gencallinterp((unsigned long long)cached_interpreter_table.BNE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNE, 1);
        return;
    }

    genbne_test();
    gendelayslot();
    gentest();
#endif
}

void genbne_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[5]);
#endif
#ifdef INTERPRET_BNE_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BNE_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNE_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BNE_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNE_IDLE, 1);
        return;
    }

    genbne_test();
    gentest_idle();
    genbne();
#endif
}

void genbnel(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[17]);
#endif
#ifdef INTERPRET_BNEL
    gencallinterp((unsigned long long)cached_interpreter_table.BNEL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNEL, 1);
        return;
    }

    genbne_test();
    free_all_registers();
    gentestl();
#endif
}

void genbnel_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[17]);
#endif
#ifdef INTERPRET_BNEL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BNEL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNEL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BNEL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BNEL_IDLE, 1);
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

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setle_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setle_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genblez(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[6]);
#endif
#ifdef INTERPRET_BLEZ
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZ, 1);
        return;
    }

    genblez_test();
    gendelayslot();
    gentest();
#endif
}

void genblez_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[6]);
#endif
#ifdef INTERPRET_BLEZ_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZ_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZ_IDLE, 1);
        return;
    }

    genblez_test();
    gentest_idle();
    genblez();
#endif
}

void genblezl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[18]);
#endif
#ifdef INTERPRET_BLEZL
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZL, 1);
        return;
    }

    genblez_test();
    free_all_registers();
    gentestl();
#endif
}

void genblezl_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[18]);
#endif
#ifdef INTERPRET_BLEZL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLEZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLEZL_IDLE, 1);
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

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setg_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setg_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genbgtz(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[7]);
#endif
#ifdef INTERPRET_BGTZ
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZ, 1);
        return;
    }

    genbgtz_test();
    gendelayslot();
    gentest();
#endif
}

void genbgtz_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[7]);
#endif
#ifdef INTERPRET_BGTZ_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZ_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZ_IDLE, 1);
        return;
    }

    genbgtz_test();
    gentest_idle();
    genbgtz();
#endif
}

void genbgtzl(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[19]);
#endif
#ifdef INTERPRET_BGTZL
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZL, 1);
        return;
    }

    genbgtz_test();
    free_all_registers();
    gentestl();
#endif
}

void genbgtzl_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[19]);
#endif
#ifdef INTERPRET_BGTZL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGTZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGTZL_IDLE, 1);
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

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setl_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rs_64bit == -1)
    {
        cmp_m32rel_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        setl_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setl_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genbltz(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[47]);
#endif
#ifdef INTERPRET_BLTZ
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZ, 1);
        return;
    }

    genbltz_test();
    gendelayslot();
    gentest();
#endif
}

void genbltz_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[47]);
#endif
#ifdef INTERPRET_BLTZ_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZ_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZ_IDLE, 1);
        return;
    }

    genbltz_test();
    gentest_idle();
    genbltz();
#endif
}

void genbltzal(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[51]);
#endif
#ifdef INTERPRET_BLTZAL
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[51]);
#endif
#ifdef INTERPRET_BLTZAL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZAL_IDLE, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[49]);
#endif
#ifdef INTERPRET_BLTZL
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZL, 1);
        return;
    }

    genbltz_test();
    free_all_registers();
    gentestl();
#endif
}

void genbltzl_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[49]);
#endif
#ifdef INTERPRET_BLTZL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZL_IDLE, 1);
        return;
    }

    genbltz_test();
    gentest_idle();
    genbltzl();
#endif
}

void genbltzall(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[53]);
#endif
#ifdef INTERPRET_BLTZALL
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[53]);
#endif
#ifdef INTERPRET_BLTZALL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BLTZALL_IDLE, 1);
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

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs);
        cmp_reg32_imm32(rs, 0);
        setge_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else if (rs_64bit == -1)
    {
        cmp_m32rel_imm32(((unsigned int *)g_dev.r4300.recomp.dst->f.i.rs)+1, 0);
        setge_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)g_dev.r4300.recomp.dst->f.i.rs);
        cmp_reg64_imm8(rs, 0);
        setge_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
    }
}

void genbgez(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[48]);
#endif
#ifdef INTERPRET_BGEZ
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZ, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZ, 1);
        return;
    }

    genbgez_test();
    gendelayslot();
    gentest();
#endif
}

void genbgez_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[48]);
#endif
#ifdef INTERPRET_BGEZ_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZ_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZ_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZ_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZ_IDLE, 1);
        return;
    }

    genbgez_test();
    gentest_idle();
    genbgez();
#endif
}

void genbgezal(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[52]);
#endif
#ifdef INTERPRET_BGEZAL
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[52]);
#endif
#ifdef INTERPRET_BGEZAL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZAL_IDLE, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[50]);
#endif
#ifdef INTERPRET_BGEZL
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZL, 1);
        return;
    }

    genbgez_test();
    free_all_registers();
    gentestl();
#endif
}

void genbgezl_out(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[50]);
#endif
#ifdef INTERPRET_BGEZL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZL_IDLE, 1);
        return;
    }

    genbgez_test();
    gentest_idle();
    genbgezl();
#endif
}

void genbgezall(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[54]);
#endif
#ifdef INTERPRET_BGEZALL
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[54]);
#endif
#ifdef INTERPRET_BGEZALL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BGEZALL_IDLE, 1);
        return;
    }

    genbgez_test();
    genbranchlink();
    gentest_idle();
    genbgezall();
#endif
}





/* global functions */


static void genbc1f_test(void)
{
    test_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000);
    sete_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
}

void genbc1f(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[100]);
#endif
#ifdef INTERPRET_BC1F
    gencallinterp((unsigned long long)cached_interpreter_table.BC1F, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC &&
                (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1F, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[100]);
#endif
#ifdef INTERPRET_BC1F_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BC1F_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC &&
                (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1F_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BC1F_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC &&
                (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1F_IDLE, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[102]);
#endif
#ifdef INTERPRET_BC1FL
    gencallinterp((unsigned long long)cached_interpreter_table.BC1FL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1FL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[102]);
#endif
#ifdef INTERPRET_BC1FL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BC1FL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1FL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BC1FL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1FL_IDLE, 1);
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
    test_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000);
    setne_m8rel((unsigned char *) &g_dev.r4300.branch_taken);
}

void genbc1t(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[101]);
#endif
#ifdef INTERPRET_BC1T
    gencallinterp((unsigned long long)cached_interpreter_table.BC1T, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1T, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[101]);
#endif
#ifdef INTERPRET_BC1T_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BC1T_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1T_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BC1T_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1T_IDLE, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[103]);
#endif
#ifdef INTERPRET_BC1TL
    gencallinterp((unsigned long long)cached_interpreter_table.BC1TL, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1TL, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[103]);
#endif
#ifdef INTERPRET_BC1TL_OUT
    gencallinterp((unsigned long long)cached_interpreter_table.BC1TL_OUT, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1TL_OUT, 1);
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
    gencallinterp((unsigned long long)cached_interpreter_table.BC1TL_IDLE, 1);
#else
    if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC
        && (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))
        || g_dev.r4300.recomp.no_compiled_jump)
    {
        gencallinterp((unsigned long long)cached_interpreter_table.BC1TL_IDLE, 1);
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[108]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.ERET, 1);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct())), (unsigned int)(dst));
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
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[63]);
#endif
#ifdef INTERPRET_SYSCALL
    gencallinterp((unsigned long long)cached_interpreter_table.SYSCALL, 0);
#else
    free_registers_move_start();

    mov_m32rel_imm32(&r4300_cp0_regs()[CP0_CAUSE_REG], 8 << 2);
    gencallinterp((unsigned long long)dynarec_exception_general, 0);
#endif
}

/* Exception instructions */

void genteq(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[96]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.TEQ, 0);
}

/* TLB instructions */

void gentlbp(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[105]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.TLBP, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct())), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBP));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

void gentlbr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[106]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.TLBR, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct())), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBR));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

void gentlbwr(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[107]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.TLBWR, 0);
}

void gentlbwi(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[104]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.TLBWI, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct())), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBWI));
    call_reg32(EAX);
    genupdate_system(0);
#endif
}

/* CP0 load/store instructions */

void genmfc0(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[109]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.MFC0, 0);
}

void genmtc0(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[110]);
#endif
    gencallinterp((unsigned long long)cached_interpreter_table.MTC0, 0);
}

/* CP1 load/store instructions */

void genlwc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[39]);
#endif
#ifdef INTERPRET_LWC1
    gencallinterp((unsigned long long)cached_interpreter_table.LWC1, 0);
#else
    gencheck_cop1_unusable();

    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.readmem);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) read_rdram);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(49);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft])); // 7
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), RDX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(EAX, RBX, RSI); // 3
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft])); // 7
    mov_preg64_reg32(RBX, EAX); // 2
#endif
}

void genldc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[40]);
#endif
#ifdef INTERPRET_LDC1
    gencallinterp((unsigned long long)cached_interpreter_table.LDC1, 0);
#else
    gencheck_cop1_unusable();

    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.readmemd);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) read_rdramd);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(49);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft])); // 7
    mov_m64rel_xreg64((unsigned long long *)(&g_dev.r4300.rdword), RDX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    jmp_imm_short(39); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg64_preg64preg64(RAX, RBX, RSI); // 4
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft])); // 7
    mov_preg64pimm32_reg32(RBX, 4, EAX); // 6
    shr_reg64_imm8(RAX, 32); // 4
    mov_preg64_reg32(RBX, EAX); // 2
#endif
}

void genswc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[43]);
#endif
#ifdef INTERPRET_SWC1
    gencallinterp((unsigned long long)cached_interpreter_table.SWC1, 0);
#else
    gencheck_cop1_unusable();

    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft]));
    mov_reg32_preg64(ECX, RDX);
    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writemem);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdram);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(49);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wword()), ECX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(21); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64_reg32(RBX, RSI, ECX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void gensdc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[44]);
#endif
#ifdef INTERPRET_SDC1
    gencallinterp((unsigned long long)cached_interpreter_table.SDC1, 0);
#else
    gencheck_cop1_unusable();

    mov_xreg64_m64rel(RSI, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft]));
    mov_reg32_preg64(ECX, RSI);
    mov_reg32_preg64pimm32(EDX, RSI, 4);
    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
    add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);
    mov_reg64_imm64(RSI, (unsigned long long) g_dev.mem.writememd);
    if (g_dev.r4300.recomp.fast_memory)
    {
        and_eax_imm32(0xDF800000);
        cmp_eax_imm32(0x80000000);
    }
    else
    {
        mov_reg64_imm64(RDI, (unsigned long long) write_rdramd);
        shr_reg32_imm8(EAX, 16);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        cmp_reg64_reg64(RAX, RDI);
    }
    je_rj(56);

    mov_reg64_imm64(RAX, (unsigned long long) (g_dev.r4300.recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct())), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address()), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword()), ECX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword())+1, EDX); // 7
    shr_reg32_imm8(EBX, 16); // 3
    mov_reg64_preg64x8preg64(RBX, RBX, RSI);  // 4
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address())); // 7
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.ri.rdram.dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64pimm32_reg32(RBX, RSI, 4, ECX); // 7
    mov_preg64preg64_reg32(RBX, RSI, EDX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) g_dev.r4300.cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) g_dev.r4300.cached_interp.blocks); // 10
    mov_reg32_reg32(ECX, EBX); // 2
    mov_reg64_preg64x8preg64(RBX, RBX, RDI);  // 4
    mov_reg64_preg64pimm32(RBX, RBX, (int) offsetof(struct precomp_block, block)); // 7
    mov_reg64_imm64(RDI, (unsigned long long) cached_interpreter_table.NOTCOMPILED); // 10
    and_eax_imm32(0xFFF); // 5
    shr_reg32_imm8(EAX, 2); // 3
    mov_reg32_imm32(EDX, sizeof(struct precomp_instr)); // 5
    mul_reg32(EDX); // 2
    mov_reg64_preg64preg64pimm32(RAX, RAX, RBX, (int) offsetof(struct precomp_instr, ops)); // 8
    cmp_reg64_reg64(RAX, RDI); // 3
    je_rj(4); // 2
    mov_preg64preg64_imm8(RCX, RSI, 1); // 4
#endif
}

void genmfc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[111]);
#endif
#ifdef INTERPRET_MFC1
    gencallinterp((unsigned long long)cached_interpreter_table.MFC1, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_reg32_preg64(EBX, RAX);
    mov_m32rel_xreg32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt, EBX);
    sar_reg32_imm8(EBX, 31);
    mov_m32rel_xreg32(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1, EBX);
#endif
}

void gendmfc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[112]);
#endif
#ifdef INTERPRET_DMFC1
    gencallinterp((unsigned long long)cached_interpreter_table.DMFC1, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *) (&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_reg32_preg64(EBX, RAX);
    mov_reg32_preg64pimm32(ECX, RAX, 4);
    mov_m32rel_xreg32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt, EBX);
    mov_m32rel_xreg32(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1, ECX);
#endif
}

void gencfc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[113]);
#endif
#ifdef INTERPRET_CFC1
    gencallinterp((unsigned long long)cached_interpreter_table.CFC1, 0);
#else
    gencheck_cop1_unusable();
    if (g_dev.r4300.recomp.dst->f.r.nrd == 31) {
        mov_xreg32_m32rel(EAX, (unsigned int*)&(*r4300_cp1_fcr31()));
    }
    else {
        mov_xreg32_m32rel(EAX, (unsigned int*)&(*r4300_cp1_fcr0()));
    }
    mov_m32rel_xreg32((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt, EAX);
    sar_reg32_imm8(EAX, 31);
    mov_m32rel_xreg32(((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1, EAX);
#endif
}

void genmtc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[114]);
#endif
#ifdef INTERPRET_MTC1
    gencallinterp((unsigned long long)cached_interpreter_table.MTC1, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg32_m32rel(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_preg64_reg32(RBX, EAX);
#endif
}

void gendmtc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[115]);
#endif
#ifdef INTERPRET_DMTC1
    gencallinterp((unsigned long long)cached_interpreter_table.DMTC1, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg32_m32rel(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_xreg32_m32rel(EBX, ((unsigned int*)g_dev.r4300.recomp.dst->f.r.rt)+1);
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.r.nrd]));
    mov_preg64_reg32(RDX, EAX);
    mov_preg64pimm32_reg32(RDX, 4, EBX);
#endif
}

void genctc1(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[116]);
#endif
#ifdef INTERPRET_CTC1
    gencallinterp((unsigned long long)cached_interpreter_table.CTC1, 0);
#else
    gencheck_cop1_unusable();

    if (g_dev.r4300.recomp.dst->f.r.nrd != 31) {
        return;
    }
    mov_xreg32_m32rel(EAX, (unsigned int*)g_dev.r4300.recomp.dst->f.r.rt);
    mov_m32rel_xreg32((unsigned int*)&(*r4300_cp1_fcr31()), EAX);
    and_eax_imm32(3);

    cmp_eax_imm32(0);
    jne_rj(13);
    mov_m32rel_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0x33F); // 11
    jmp_imm_short(51); // 2

    cmp_eax_imm32(1); // 5
    jne_rj(13); // 2
    mov_m32rel_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0xF3F); // 11
    jmp_imm_short(31); // 2

    cmp_eax_imm32(2); // 5
    jne_rj(13); // 2
    mov_m32rel_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0xB3F); // 11
    jmp_imm_short(11); // 2

    mov_m32rel_imm32((unsigned int*)&g_dev.r4300.cp1.rounding_mode, 0x73F); // 11

    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

/* CP1 computational instructions */

void genabs_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[124]);
#endif
#ifdef INTERPRET_ABS_S
    gencallinterp((unsigned long long)cached_interpreter_table.ABS_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fabs_();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genabs_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[124]);
#endif
#ifdef INTERPRET_ABS_D
    gencallinterp((unsigned long long)cached_interpreter_table.ABS_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fabs_();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genadd_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[119]);
#endif
#ifdef INTERPRET_ADD_S
    gencallinterp((unsigned long long)cached_interpreter_table.ADD_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fadd_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genadd_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[119]);
#endif
#ifdef INTERPRET_ADD_D
    gencallinterp((unsigned long long)cached_interpreter_table.ADD_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fadd_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gendiv_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[122]);
#endif
#ifdef INTERPRET_DIV_S
    gencallinterp((unsigned long long)cached_interpreter_table.DIV_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fdiv_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gendiv_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[122]);
#endif
#ifdef INTERPRET_DIV_D
    gencallinterp((unsigned long long)cached_interpreter_table.DIV_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fdiv_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genmov_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[125]);
#endif
#ifdef INTERPRET_MOV_S
    gencallinterp((unsigned long long)cached_interpreter_table.MOV_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    mov_reg32_preg64(EBX, RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    mov_preg64_reg32(RAX, EBX);
#endif
}

void genmov_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[125]);
#endif
#ifdef INTERPRET_MOV_D
    gencallinterp((unsigned long long)cached_interpreter_table.MOV_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    mov_reg32_preg64(EBX, RAX);
    mov_reg32_preg64pimm32(ECX, RAX, 4);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    mov_preg64_reg32(RAX, EBX);
    mov_preg64pimm32_reg32(RAX, 4, ECX);
#endif
}

void genmul_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[121]);
#endif
#ifdef INTERPRET_MUL_S
    gencallinterp((unsigned long long)cached_interpreter_table.MUL_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fmul_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genmul_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[121]);
#endif
#ifdef INTERPRET_MUL_D
    gencallinterp((unsigned long long)cached_interpreter_table.MUL_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fmul_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genneg_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[126]);
#endif
#ifdef INTERPRET_NEG_S
    gencallinterp((unsigned long long)cached_interpreter_table.NEG_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fchs();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genneg_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[126]);
#endif
#ifdef INTERPRET_NEG_D
    gencallinterp((unsigned long long)cached_interpreter_table.NEG_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fchs();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gensqrt_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[123]);
#endif
#ifdef INTERPRET_SQRT_S
    gencallinterp((unsigned long long)cached_interpreter_table.SQRT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fsqrt();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gensqrt_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[123]);
#endif
#ifdef INTERPRET_SQRT_D
    gencallinterp((unsigned long long)cached_interpreter_table.SQRT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fsqrt();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gensub_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[120]);
#endif
#ifdef INTERPRET_SUB_S
    gencallinterp((unsigned long long)cached_interpreter_table.SUB_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fsub_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gensub_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[120]);
#endif
#ifdef INTERPRET_SUB_D
    gencallinterp((unsigned long long)cached_interpreter_table.SUB_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fsub_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gentrunc_w_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_W_S
    gencallinterp((unsigned long long)cached_interpreter_table.TRUNC_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_w_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_W_D
    gencallinterp((unsigned long long)cached_interpreter_table.TRUNC_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_l_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_L_S
    gencallinterp((unsigned long long)cached_interpreter_table.TRUNC_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gentrunc_l_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_L_D
    gencallinterp((unsigned long long)cached_interpreter_table.TRUNC_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_w_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_W_S
    gencallinterp((unsigned long long)cached_interpreter_table.ROUND_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_w_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_W_D
    gencallinterp((unsigned long long)cached_interpreter_table.ROUND_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_l_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_L_S
    gencallinterp((unsigned long long)cached_interpreter_table.ROUND_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genround_l_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_L_D
    gencallinterp((unsigned long long)cached_interpreter_table.ROUND_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_w_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_W_S
    gencallinterp((unsigned long long)cached_interpreter_table.CEIL_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_w_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_W_D
    gencallinterp((unsigned long long)cached_interpreter_table.CEIL_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_l_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_L_S
    gencallinterp((unsigned long long)cached_interpreter_table.CEIL_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genceil_l_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_L_D
    gencallinterp((unsigned long long)cached_interpreter_table.CEIL_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_w_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_W_S
    gencallinterp((unsigned long long)cached_interpreter_table.FLOOR_W_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_w_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_W_D
    gencallinterp((unsigned long long)cached_interpreter_table.FLOOR_W_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_l_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_L_S
    gencallinterp((unsigned long long)cached_interpreter_table.FLOOR_L_S, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void genfloor_l_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_L_D
    gencallinterp((unsigned long long)cached_interpreter_table.FLOOR_L_D, 0);
#else
    gencheck_cop1_unusable();
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&g_dev.r4300.cp1.rounding_mode);
#endif
}

void gencvt_s_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_D
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_S_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_s_w(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_W
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_S_W, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_s_l(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_L
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_S_L, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_d_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_S
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_D_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_d_w(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_W
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_D_W, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_d_l(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_L
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_D_L, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fild_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_w_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_W_S
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_W_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
#endif
}

void gencvt_w_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_W_D
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_W_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
#endif
}

void gencvt_l_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_L_S
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_L_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
#endif
}

void gencvt_l_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_L_D
    gencallinterp((unsigned long long)cached_interpreter_table.CVT_L_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
#endif
}

/* CP1 relational instructions */

void genc_f_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_F_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_F_S, 0);
#else
    gencheck_cop1_unusable();
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000);
#endif
}

void genc_f_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_F_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_F_D, 0);
#else
    gencheck_cop1_unusable();
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000);
#endif
}

void genc_un_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UN_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_UN_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
#endif
}

void genc_un_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UN_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_UN_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
#endif
}

void genc_eq_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_EQ_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_EQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_eq_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_EQ_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_EQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ueq_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UEQ_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_UEQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ueq_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UEQ_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_UEQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_olt_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLT_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_OLT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_olt_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLT_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_OLT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ult_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULT_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_ULT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ult_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULT_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_ULT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ole_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLE_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_OLE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ole_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLE_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_OLE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ule_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULE_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_ULE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ule_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULE_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_ULE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_sf_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SF_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_SF_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000);
#endif
}

void genc_sf_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SF_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_SF_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000);
#endif
}

void genc_ngle_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGLE_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGLE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
#endif
}

void genc_ngle_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGLE_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGLE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
#endif
}

void genc_seq_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SEQ_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_SEQ_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_seq_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SEQ_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_SEQ_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ngl_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGL_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGL_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ngl_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGL_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGL_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_lt_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LT_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_LT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_lt_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LT_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_LT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_nge_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGE_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_nge_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGE_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_le_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LE_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_LE_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_le_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LE_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_LE_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ngt_s(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGT_S
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGT_S, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}

void genc_ngt_d(void)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGT_D
    gencallinterp((unsigned long long)cached_interpreter_table.C_NGT_D, 0);
#else
    gencheck_cop1_unusable();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31()), ~0x800000); // 11
#endif
}
