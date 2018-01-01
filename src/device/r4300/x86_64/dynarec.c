/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dynarec.c                                               *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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
#include "device/rdram/rdram.h"
#include "main/main.h"

#if defined(COUNT_INSTR)
#include "device/r4300/instr_counters.h"
#endif

#include <assert.h>
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
    struct r4300_core* r4300 = &g_dev.r4300;

    if (*r4300_stop(r4300) == 1)
    {
        dyna_stop(r4300);
        return;
    }

    if ((*r4300_pc_struct(r4300))->reg_cache_infos.need_map)
    {
        *r4300->return_address = (unsigned long long) ((*r4300_pc_struct(r4300))->reg_cache_infos.jump_wrapper);
    }
    else
    {
        *r4300->return_address = (unsigned long long) (r4300->cached_interp.actual->code + (*r4300_pc_struct(r4300))->local_addr);
    }
}

void dyna_stop(struct r4300_core* r4300)
{
    if (r4300->save_rip == 0)
    {
        DebugMessage(M64MSG_WARNING, "Instruction pointer is 0 at dyna_stop()");
    }
    else
    {
        *r4300->return_address = (unsigned long long) r4300->save_rip;
    }
}


/* M64P Pseudo instructions */

static void gencheck_cop1_unusable(struct r4300_core* r4300)
{
    free_registers_move_start();

    test_m32rel_imm32((unsigned int*)&r4300_cp0_regs(&r4300->cp0)[CP0_STATUS_REG], CP0_STATUS_CU1);
    jne_rj(0);
    jump_start_rel8();

    gencallinterp(r4300, (unsigned long long)dynarec_check_cop1_unusable, 0);

    jump_end_rel8();
}

static void gencp0_update_count(struct r4300_core* r4300, unsigned int addr)
{
#if !defined(COMPARE_CORE) && !defined(DBG)
    mov_reg32_imm32(EAX, addr);
    sub_xreg32_m32rel(EAX, (unsigned int*)(&r4300->cp0.last_addr));
    shr_reg32_imm8(EAX, 2);
    mov_xreg32_m32rel(EDX, (void*)&r4300->cp0.count_per_op);
    mul_reg32(EDX);
    add_m32rel_xreg32((unsigned int*)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]), EAX);
#else
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long)dynarec_cp0_update_count);
    call_reg64(RAX);
#endif
}

static void gencheck_interrupt(struct r4300_core* r4300, unsigned long long instr_structure)
{
    mov_xreg32_m32rel(EAX, (void*)(r4300_cp0_next_interrupt(&r4300->cp0)));
    cmp_xreg32_m32rel(EAX, (void*)&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(RAX, (unsigned long long) instr_structure);
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void gencheck_interrupt_out(struct r4300_core* r4300, unsigned int addr)
{
    mov_xreg32_m32rel(EAX, (void*)(r4300_cp0_next_interrupt(&r4300->cp0)));
    cmp_xreg32_m32rel(EAX, (void*)&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_m32rel_imm32((unsigned int*)(&r4300->fake_instr.addr), addr);
    mov_reg64_imm64(RAX, (unsigned long long) (&r4300->fake_instr));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void gencheck_interrupt_reg(struct r4300_core* r4300) // addr is in EAX
{
    mov_xreg32_m32rel(EBX, (void*)r4300_cp0_next_interrupt(&r4300->cp0));
    cmp_xreg32_m32rel(EBX, (void*)&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]);
    ja_rj(0);
    jump_start_rel8();

    mov_m32rel_xreg32((unsigned int*)(&r4300->fake_instr.addr), EAX);
    mov_reg64_imm64(RAX, (unsigned long long) (&r4300->fake_instr));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_gen_interrupt);
    call_reg64(RAX);

    jump_end_rel8();
}

static void ld_register_alloc(struct r4300_core* r4300, int *pGpr1, int *pGpr2, int *pBase1, int *pBase2)
{
    int gpr1, gpr2, base1, base2 = 0;

#ifdef COMPARE_CORE
    free_registers_move_start(); // to maintain parity with 32-bit core
#endif

    if (r4300->recomp.dst->f.i.rs == r4300->recomp.dst->f.i.rt)
    {
        allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);          // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)r4300->recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        gpr2 = lock_register(lru_register());                      // free and lock least recently used register for usage here
        add_reg32_imm32(gpr1, (int)r4300->recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr2, gpr1);
    }
    else
    {
        gpr2 = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);   // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)r4300->recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        free_register(gpr2);                                       // write out gpr2 if dirty because I'm going to trash it right now
        add_reg32_imm32(gpr2, (int)r4300->recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr1, gpr2);
        lock_register(gpr2);                                       // lock the freed gpr2 it so it doesn't get returned in the lru query
    }
    base1 = lock_register(lru_base_register());                  // get another lru register
    if (!r4300->recomp.fast_memory)
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

static void ld_register_alloc2(struct r4300_core* r4300, int *pGpr1, int *pGpr2, int *pBase1, int *pBase2)
{
    int gpr1, gpr2, base1, base2;

#ifdef COMPARE_CORE
    free_registers_move_start(); // to maintain parity with 32-bit core
#endif

    base2 = lock_register(ECX); // make sure we get ECX for base2

    if (r4300->recomp.dst->f.i.rs == r4300->recomp.dst->f.i.rt)
    {
        allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);          // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)r4300->recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        gpr2 = lock_register(lru_register());                      // free and lock least recently used register for usage here
        add_reg32_imm32(gpr1, (int)r4300->recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr2, gpr1);
    }
    else
    {
        gpr2 = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);   // tell regcache we need to read RS register here
        gpr1 = allocate_register_32_w((unsigned int*)r4300->recomp.dst->f.r.rt); // tell regcache we will modify RT register during this instruction
        free_register(gpr2);                                       // write out gpr2 if dirty because I'm going to trash it right now
        add_reg32_imm32(gpr2, (int)r4300->recomp.dst->f.i.immediate);
        mov_reg32_reg32(gpr1, gpr2);
        lock_register(gpr2);                                       // lock the freed gpr2 it so it doesn't get returned in the lru query
    }
    base1 = lock_register(lru_base_register());                  // get another lru register
    unlock_register(base2);
    unlock_register(base1);                                      // unlock the locked registers (they are
    unlock_register(gpr2);
    set_register_state(gpr1, NULL, 0, 0);                        // clear gpr1 state because it hasn't been written yet -
    // we don't want it to be pushed/popped around read_rdramX call
    *pGpr1 = gpr1;
    *pGpr2 = gpr2;
    *pBase1 = base1;
    *pBase2 = base2;

    /* base2 is RCX */
    assert(gpr1 != RCX);
    assert(gpr2 != RCX);
    assert(base1 != RCX);
    assert(base2 == RCX);
}

#ifdef COMPARE_CORE
extern unsigned int op; /* api/debugger.c */

void gendebug(struct r4300_core* r4300)
{
    free_all_registers();

    mov_memoffs64_rax((unsigned long long *) &r4300->debug_reg_storage);
    mov_reg64_imm64(RAX, (unsigned long long) &r4300->debug_reg_storage);
    mov_preg64pimm8_reg64(RAX,  8, RBX);
    mov_preg64pimm8_reg64(RAX, 16, RCX);
    mov_preg64pimm8_reg64(RAX, 24, RDX);
    mov_preg64pimm8_reg64(RAX, 32, RSP);
    mov_preg64pimm8_reg64(RAX, 40, RBP);
    mov_preg64pimm8_reg64(RAX, 48, RSI);
    mov_preg64pimm8_reg64(RAX, 56, RDI);

    mov_reg64_imm64(RAX, (unsigned long long) r4300->recomp.dst);
    mov_memoffs64_rax((unsigned long long *) &(*r4300_pc_struct(r4300)));
    mov_reg32_imm32(EAX, (unsigned int) r4300->recomp.src);
    mov_memoffs32_eax((unsigned int *) &op);
    mov_reg64_imm64(RAX, (unsigned long long) CoreCompareCallback);
    call_reg64(RAX);

    mov_reg64_imm64(RAX, (unsigned long long) &r4300->debug_reg_storage);
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

void genni(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[1]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.NI, 0);
}

void gennotcompiled(struct r4300_core* r4300)
{
    free_registers_move_start();

    mov_reg64_imm64(RAX, (unsigned long long) r4300->recomp.dst);
    mov_memoffs64_rax((unsigned long long *) &(*r4300_pc_struct(r4300))); /* RIP-relative will not work here */
    mov_reg64_imm64(RAX, (unsigned long long) cached_interpreter_table.NOTCOMPILED);
    call_reg64(RAX);
}

void genlink_subblock(struct r4300_core* r4300)
{
    free_all_registers();
    jmp(r4300->recomp.dst->addr+4);
}

void genfin_block(struct r4300_core* r4300)
{
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.FIN_BLOCK, 0);
}

void gencallinterp(struct r4300_core* r4300, uintptr_t addr, int jump)
{
    free_registers_move_start();

    if (jump) {
        mov_m32rel_imm32((unsigned int*)(&r4300->dyna_interp), 1);
    }

    mov_reg64_imm64(RAX, (unsigned long long) r4300->recomp.dst);
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, addr);
    call_reg64(RAX);

    if (jump)
    {
        mov_m32rel_imm32((unsigned int*)(&r4300->dyna_interp), 0);
        mov_reg64_imm64(RAX, (unsigned long long)dyna_jump);
        call_reg64(RAX);
    }
}

void gendelayslot(struct r4300_core* r4300)
{
    mov_m32rel_imm32((void*)(&r4300->delay_slot), 1);
    recompile_opcode(r4300);

    free_all_registers();
    gencp0_update_count(r4300, r4300->recomp.dst->addr+4);

    mov_m32rel_imm32((void*)(&r4300->delay_slot), 0);
}

/* Reserved */

void genreserved(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[0]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.RESERVED, 0);
}

/* Load instructions */

void genlb(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[24]);
#endif
#ifdef INTERPRET_LB
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LB, 0);
#else
    free_registers_move_start();

    ld_register_alloc2(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    /* if non RDRAM read,
     * compute shift (base2) and address (gpr2) to perform a regular read.
     * Save base2 content to memory as RCX can be overwritten when calling read32 function */
    mov_reg64_reg64(base2, gpr2);
    and_reg64_imm8(base2, 3);
    xor_reg8_imm8(base2, 3);
    shl_reg64_imm8(base2, 3);
    mov_m64rel_xreg64(&r4300->recomp.shift, base2);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr2, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr2);
    and_reg64_reg64(RAX, RAX);
    je_rj(57);

    mov_xreg64_m64rel(gpr1, (unsigned long long*)r4300->recomp.dst->f.i.rt); // 7
    mov_xreg64_m64rel(RCX, &r4300->recomp.shift); // 7
    shr_reg64_cl(gpr1); // 3
    mov_m64rel_xreg64((unsigned long long*)r4300->recomp.dst->f.i.rt, gpr1); // 7
    movsx_xreg32_m8rel(gpr1, (unsigned char *)r4300->recomp.dst->f.i.rt); // 7
    jmp_imm_short(24); // 2

    /* else (RDRAM read), read byte */
    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 3); // 4
    movsx_reg32_8preg64preg64(gpr1, gpr2, base1); // 4

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlbu(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[28]);
#endif
#ifdef INTERPRET_LBU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LBU, 0);
#else
    free_registers_move_start();

    ld_register_alloc2(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    /* if non RDRAM read,
     * compute shift (base2) and address (gpr2) to perform a regular read.
     * Save base2 content to memory as RCX can be overwritten when calling read32 function */
    mov_reg64_reg64(base2, gpr2);
    and_reg64_imm8(base2, 3);
    xor_reg8_imm8(base2, 3);
    shl_reg64_imm8(base2, 3);
    mov_m64rel_xreg64(&r4300->recomp.shift, base2);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr2, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr2);
    and_reg64_reg64(RAX, RAX);
    je_rj(48);

    mov_xreg32_m32rel(gpr1, (unsigned int *)r4300->recomp.dst->f.i.rt); // 7
    mov_xreg64_m64rel(RCX, &r4300->recomp.shift); // 7
    shr_reg64_cl(gpr1); // 3
    jmp_imm_short(23); // 2

    /* else (RDRAM read), read byte */
    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 3); // 4
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    and_reg32_imm32(gpr1, 0xFF); // 6

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlh(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[25]);
#endif
#ifdef INTERPRET_LH
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LH, 0);
#else
    free_registers_move_start();

    ld_register_alloc2(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    /* if non RDRAM read,
     * compute shift (base2) and address (gpr2) to perform a regular read.
     * Save base2 content to memory as RCX can be overwritten when calling read32 function */
    mov_reg64_reg64(base2, gpr2);
    and_reg64_imm8(base2, 2);
    xor_reg8_imm8(base2, 2);
    shl_reg64_imm8(base2, 3);
    mov_m64rel_xreg64(&r4300->recomp.shift, base2);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr2, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr2);
    and_reg64_reg64(RAX, RAX);
    je_rj(58);

    mov_xreg64_m64rel(gpr1, (unsigned long long*)r4300->recomp.dst->f.i.rt); // 7
    mov_xreg64_m64rel(RCX, &r4300->recomp.shift); // 7
    shr_reg64_cl(gpr1); // 3
    mov_m64rel_xreg64((unsigned long long*)r4300->recomp.dst->f.i.rt, gpr1); // 7
    movsx_xreg32_m16rel(gpr1, (unsigned short *)r4300->recomp.dst->f.i.rt); // 8
    jmp_imm_short(24); // 2

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 2); // 4
    movsx_reg32_16preg64preg64(gpr1, gpr2, base1); // 4

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 0);
#endif
}

void genlhu(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[29]);
#endif
#ifdef INTERPRET_LHU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LHU, 0);
#else
    free_registers_move_start();

    ld_register_alloc2(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    /* if non RDRAM read,
     * compute shift (base2) and address (gpr2) to perform a regular read.
     * Save base2 content to memory as RCX can be overwritten when calling read32 function */
    mov_reg64_reg64(base2, gpr2);
    and_reg64_imm8(base2, 2);
    xor_reg8_imm8(base2, 2);
    shl_reg64_imm8(base2, 3);
    mov_m64rel_xreg64(&r4300->recomp.shift, base2);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr2, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr2);
    and_reg64_reg64(RAX, RAX);
    je_rj(48);

    mov_xreg32_m32rel(gpr1, (unsigned int *)r4300->recomp.dst->f.i.rt); // 7
    mov_xreg64_m64rel(RCX, &r4300->recomp.shift); // 7
    shr_reg64_cl(gpr1); // 3
    jmp_imm_short(23); // 2

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    xor_reg8_imm8(gpr2, 2); // 4
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    and_reg32_imm32(gpr1, 0xFFFF); // 6

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 0);
#endif
}

void genll(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[42]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LL, 0);
}

void genlw(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[26]);
#endif
#ifdef INTERPRET_LW
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LW, 0);
#else
    free_registers_move_start();

    ld_register_alloc(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    jne_rj(21);

    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3
    jmp_imm_short(0); // 2
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr1, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr1);
    mov_xreg32_m32rel(gpr1, (unsigned int *)(r4300->recomp.dst->f.i.rt));

    jump_end_rel8();

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 0);     // set gpr1 state as dirty, and bound to r4300 reg RT
#endif
}

void genlwu(struct r4300_core* r4300)
{
    int gpr1, gpr2, base1, base2 = 0;
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[30]);
#endif
#ifdef INTERPRET_LWU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LWU, 0);
#else
    free_registers_move_start();

    ld_register_alloc(r4300, &gpr1, &gpr2, &base1, &base2);

    /* is address in RDRAM ? */
    and_reg32_imm32(gpr1, 0xDF800000);
    cmp_reg32_imm32(gpr1, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(gpr1, 16);
        and_reg32_imm32(gpr1, 0x1fff);
        lea_reg64_preg64x2preg64(gpr1, gpr1, gpr1);
        mov_reg64_imm64(base1, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(gpr1, gpr1, base1);
        mov_reg64_imm64(base1, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(gpr1, base1);

        jump_end_rel8();
    }
    je_rj(0);
    jump_start_rel8();

    mov_reg64_imm64(gpr1, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), gpr1);
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), gpr2);
    mov_reg64_imm64(gpr1, (unsigned long long) r4300->recomp.dst->f.i.rt);
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), gpr1);
    mov_reg64_imm64(gpr2, (unsigned long long)dynarec_read_aligned_word);
    call_reg64(gpr2);
    mov_xreg32_m32rel(gpr1, (unsigned int *)r4300->recomp.dst->f.i.rt);
    jmp_imm_short(19);

    jump_end_rel8();
    mov_reg64_imm64(base1, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(gpr2, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(gpr1, gpr2, base1); // 3

    set_register_state(gpr1, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 1);
#endif
}

void genlwl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[27]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LWL, 0);
}

void genlwr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[31]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LWR, 0);
}

void genld(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[41]);
#endif
#ifdef INTERPRET_LD
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LD, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    add_eax_imm32((int)r4300->recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(62);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_reg64_imm64(RAX, (unsigned long long) r4300->recomp.dst->f.i.rt); // 10
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), RAX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_read_aligned_dword); // 10
    call_reg64(RBX); // 2
    mov_xreg64_m64rel(RAX, (unsigned long long *)(r4300->recomp.dst->f.i.rt)); // 7
    jmp_imm_short(33); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(EAX, RBX, RSI); // 3
    mov_reg32_preg64preg64pimm32(EBX, RBX, RSI, 4); // 7
    shl_reg64_imm8(RAX, 32); // 4
    or_reg64_reg64(RAX, RBX); // 3

    set_register_state(RAX, (unsigned int*)r4300->recomp.dst->f.i.rt, 1, 1);
#endif
}

void genldl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[22]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LDL, 0);
}

void genldr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[23]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LDR, 0);
}

/* Store instructions */

void gensb(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[32]);
#endif
#ifdef INTERPRET_SB
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SB, 0);
#else
    free_registers_move_start();

    /* get value in EDX */
    xor_reg64_reg64(RDX, RDX);
    mov_xreg8_m8rel(DL, (unsigned char *)r4300->recomp.dst->f.i.rt);
    /* get address in both EAX and EBX */
    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    add_eax_imm32((int)r4300->recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(88);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    /* if non RDRAM write,
     * compute shift (ECX), word (EDX), wmask (EDX) and address (EBX) to perform a regular write */
    mov_reg32_reg32(ECX, EBX); // 2
    and_reg32_imm32(ECX, 3); // 6
    xor_reg8_imm8(CL, 3); // 4
    shl_reg32_imm8(ECX, 3); // 3
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    shl_reg32_cl(EDX); // 2
    mov_m32rel_xreg32((unsigned int *)(r4300_wword(r4300)), EDX); // 7
    mov_reg64_imm64(RDX, 0xff); // 10
    shl_reg32_cl(EDX); // 2
    mov_m32rel_xreg32((unsigned int *)(r4300_wmask(r4300)), EDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_word); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(25); // 2

    /* else (RDRAM write), write byte */
    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 3); // 4
    mov_preg64preg64_reg8(RBX, RSI, DL); // 3

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void gensh(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[33]);
#endif
#ifdef INTERPRET_SH
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SH, 0);
#else
    free_registers_move_start();

    /* get value in EDX */
    xor_reg64_reg64(RDX, RDX);
    mov_xreg16_m16rel(DX, (unsigned short *)r4300->recomp.dst->f.i.rt);
    /* get address in both EAX and EBX */
    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    add_eax_imm32((int)r4300->recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(88);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    /* if non RDRAM write,
     * compute shift (ECX), word (EDX), wmask (EDX) and address (EBX) to perform a regular write */
    mov_reg32_reg32(ECX, EBX); // 2
    and_reg32_imm32(ECX, 2); // 6
    xor_reg8_imm8(CL, 2); // 4
    shl_reg32_imm8(ECX, 3); // 3
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    shl_reg32_cl(EDX); // 2
    mov_m32rel_xreg32((unsigned int *)(r4300_wword(r4300)), EDX); // 7
    mov_reg64_imm64(RDX, 0xffff); // 10
    shl_reg32_cl(EDX); // 2
    mov_m32rel_xreg32((unsigned int *)(r4300_wmask(r4300)), EDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_word); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(26); // 2

    /* else (RDRAM write), write hword */
    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    xor_reg8_imm8(BL, 2); // 4
    mov_preg64preg64_reg16(RBX, RSI, DX); // 4

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void gensc(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[46]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SC, 0);
}

void gensw(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[34]);
#endif
#ifdef INTERPRET_SW
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SW, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(ECX, (unsigned int *)r4300->recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    add_eax_imm32((int)r4300->recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(63);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wword(r4300)), ECX); // 7
    mov_m32rel_imm32((unsigned int *)(r4300_wmask(r4300)), ~UINT32_C(0)); // 11
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_word); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(21); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64_reg32(RBX, RSI, ECX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void genswl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[35]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SWL, 0);
}

void genswr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[36]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SWR, 0);
}

void gensd(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[45]);
#endif
#ifdef INTERPRET_SD
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SD, 0);
#else
    free_registers_move_start();

    mov_xreg32_m32rel(ECX, (unsigned int *)r4300->recomp.dst->f.i.rt);
    mov_xreg32_m32rel(EDX, ((unsigned int *)r4300->recomp.dst->f.i.rt)+1);
    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    add_eax_imm32((int)r4300->recomp.dst->f.i.immediate);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(59);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword(r4300)), ECX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword(r4300))+1, EDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_dword); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64pimm32_reg32(RBX, RSI, 4, ECX); // 7
    mov_preg64preg64_reg32(RBX, RSI, EDX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void gensdl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[37]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SDL, 0);
}

void gensdr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[38]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SDR, 0);
}

/* Computational instructions */

void genadd(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[79]);
#endif
#ifdef INTERPRET_ADD
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADD, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void genaddu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[80]);
#endif
#ifdef INTERPRET_ADDU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADDU, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void genaddi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[8]);
#endif
#ifdef INTERPRET_ADDI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADDI, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)r4300->recomp.dst->f.i.immediate);
#endif
}

void genaddiu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[9]);
#endif
#ifdef INTERPRET_ADDIU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADDIU, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.i.rt);

    mov_reg32_reg32(rt, rs);
    add_reg32_imm32(rt,(int)r4300->recomp.dst->f.i.immediate);
#endif
}

void gendadd(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[89]);
#endif
#ifdef INTERPRET_DADD
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DADD, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendaddu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[90]);
#endif
#ifdef INTERPRET_DADDU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DADDU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendaddi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[20]);
#endif
#ifdef INTERPRET_DADDI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DADDI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    add_reg64_imm32(rt, (int) r4300->recomp.dst->f.i.immediate);
#endif
}

void gendaddiu(struct r4300_core* r4300)
{
#ifdef INTERPRET_DADDIU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DADDIU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    add_reg64_imm32(rt, (int) r4300->recomp.dst->f.i.immediate);
#endif
}

void gensub(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[81]);
#endif
#ifdef INTERPRET_SUB
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SUB, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void gensubu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[82]);
#endif
#ifdef INTERPRET_SUBU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SUBU, 0);
#else
    int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void gendsub(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[91]);
#endif
#ifdef INTERPRET_DSUB
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSUB, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendsubu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[92]);
#endif
#ifdef INTERPRET_DSUBU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSUBU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void genslt(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[87]);
#endif
#ifdef INTERPRET_SLT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLT, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    cmp_reg64_reg64(rs, rt);
    setl_reg8(rd);
    and_reg64_imm8(rd, 1);
#endif
}

void gensltu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[88]);
#endif
#ifdef INTERPRET_SLTU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLTU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    cmp_reg64_reg64(rs, rt);
    setb_reg8(rd);
    and_reg64_imm8(rd, 1);
#endif
}

void genslti(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[10]);
#endif
#ifdef INTERPRET_SLTI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLTI, 0);
#else
    int rs = allocate_register_64((unsigned long long *) r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *) r4300->recomp.dst->f.i.rt);
    int imm = (int) r4300->recomp.dst->f.i.immediate;

    cmp_reg64_imm32(rs, imm);
    setl_reg8(rt);
    and_reg64_imm8(rt, 1);
#endif
}

void gensltiu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[11]);
#endif
#ifdef INTERPRET_SLTIU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLTIU, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.i.rt);
    int imm = (int) r4300->recomp.dst->f.i.immediate;

    cmp_reg64_imm32(rs, imm);
    setb_reg8(rt);
    and_reg64_imm8(rt, 1);
#endif
}

void genand(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[83]);
#endif
#ifdef INTERPRET_AND
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.AND, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void genandi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[12]);
#endif
#ifdef INTERPRET_ANDI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ANDI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    and_reg64_imm32(rt, (unsigned short)r4300->recomp.dst->f.i.immediate);
#endif
}

void genor(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[84]);
#endif
#ifdef INTERPRET_OR
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.OR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void genori(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[13]);
#endif
#ifdef INTERPRET_ORI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ORI, 0);
#else
    int rs = allocate_register_64((unsigned long long *) r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *) r4300->recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    or_reg64_imm32(rt, (unsigned short)r4300->recomp.dst->f.i.immediate);
#endif
}

void genxor(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[85]);
#endif
#ifdef INTERPRET_XOR
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.XOR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void genxori(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[14]);
#endif
#ifdef INTERPRET_XORI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.XORI, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
    int rt = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.i.rt);

    mov_reg64_reg64(rt, rs);
    xor_reg64_imm32(rt, (unsigned short)r4300->recomp.dst->f.i.immediate);
#endif
}

void gennor(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[86]);
#endif
#ifdef INTERPRET_NOR
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.NOR, 0);
#else
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void genlui(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[15]);
#endif
#ifdef INTERPRET_LUI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LUI, 0);
#else
    int rt = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.i.rt);

    mov_reg32_imm32(rt, (unsigned int)r4300->recomp.dst->f.i.immediate << 16);
#endif
}

/* Shift instructions */

void gennop(struct r4300_core* r4300)
{
}

void gensll(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[55]);
#endif
#ifdef INTERPRET_SLL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLL, 0);
#else
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shl_reg32_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gensllv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[58]);
#endif
#ifdef INTERPRET_SLLV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SLLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void gendsll(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[93]);
#endif
#ifdef INTERPRET_DSLL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSLL, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shl_reg64_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gendsllv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[68]);
#endif
#ifdef INTERPRET_DSLLV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSLLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendsll32(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[97]);
#endif
#ifdef INTERPRET_DSLL32
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSLL32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shl_reg64_imm8(rd, r4300->recomp.dst->f.r.sa + 32);
#endif
}

void gensrl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[56]);
#endif
#ifdef INTERPRET_SRL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SRL, 0);
#else
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    shr_reg32_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gensrlv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[59]);
#endif
#ifdef INTERPRET_SRLV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SRLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void gendsrl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[94]);
#endif
#ifdef INTERPRET_DSRL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRL, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shr_reg64_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gendsrlv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[69]);
#endif
#ifdef INTERPRET_DSRLV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRLV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendsrl32(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[98]);
#endif
#ifdef INTERPRET_DSRL32
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRL32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    shr_reg64_imm8(rd, r4300->recomp.dst->f.r.sa + 32);
#endif
}

void gensra(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[57]);
#endif
#ifdef INTERPRET_SRA
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SRA, 0);
#else
    int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

    mov_reg32_reg32(rd, rt);
    sar_reg32_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gensrav(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[60]);
#endif
#ifdef INTERPRET_SRAV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SRAV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_32_w((unsigned int *)r4300->recomp.dst->f.r.rd);

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

void gendsra(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[95]);
#endif
#ifdef INTERPRET_DSRA
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRA, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    sar_reg64_imm8(rd, r4300->recomp.dst->f.r.sa);
#endif
}

void gendsrav(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[70]);
#endif
#ifdef INTERPRET_DSRAV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRAV, 0);
#else
    int rt, rd;
    allocate_register_32_manually(ECX, (unsigned int *)r4300->recomp.dst->f.r.rs);

    rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

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

void gendsra32(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[99]);
#endif
#ifdef INTERPRET_DSRA32
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DSRA32, 0);
#else
    int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rt);
    int rd = allocate_register_64_w((unsigned long long *)r4300->recomp.dst->f.r.rd);

    mov_reg64_reg64(rd, rt);
    sar_reg64_imm8(rd, r4300->recomp.dst->f.r.sa + 32);
#endif
}

/* Multiply / Divide instructions */

void genmult(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[71]);
#endif
#ifdef INTERPRET_MULT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MULT, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo(r4300)); /* these must be done first so they are not assigned by allocate_register() */
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi(r4300));
    rs = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    imul_reg32(rt);
#endif
}

void genmultu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[72]);
#endif
#ifdef INTERPRET_MULTU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MULTU, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo(r4300));
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi(r4300));
    rs = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rt);
    mov_reg32_reg32(EAX, rs);
    mul_reg32(rt);
#endif
}

void gendmult(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[75]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DMULT, 0);
}

void gendmultu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[76]);
#endif
#ifdef INTERPRET_DMULTU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DMULTU, 0);
#else
    free_registers_move_start();

    mov_xreg64_m64rel(RAX, (unsigned long long *) r4300->recomp.dst->f.r.rs);
    mov_xreg64_m64rel(RDX, (unsigned long long *) r4300->recomp.dst->f.r.rt);
    mul_reg64(RDX);
    mov_m64rel_xreg64((unsigned long long *) r4300_mult_lo(r4300), RAX);
    mov_m64rel_xreg64((unsigned long long *) r4300_mult_hi(r4300), RDX);
#endif
}

void gendiv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[73]);
#endif
#ifdef INTERPRET_DIV
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DIV, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo(r4300));
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi(r4300));
    rs = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 1 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    cdq(); // 1
    idiv_reg32(rt); // 2
#endif
}

void gendivu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[74]);
#endif
#ifdef INTERPRET_DIVU
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DIVU, 0);
#else
    int rs, rt;
    allocate_register_32_manually_w(EAX, (unsigned int *)r4300_mult_lo(r4300));
    allocate_register_32_manually_w(EDX, (unsigned int *)r4300_mult_hi(r4300));
    rs = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rs);
    rt = allocate_register_32((unsigned int*)r4300->recomp.dst->f.r.rt);
    cmp_reg32_imm32(rt, 0);
    je_rj((rs == EAX ? 0 : 2) + 2 + 2);
    mov_reg32_reg32(EAX, rs); // 0 or 2
    xor_reg32_reg32(EDX, EDX); // 2
    div_reg32(rt); // 2
#endif
}

void genddiv(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[77]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DDIV, 0);
}

void genddivu(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[78]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DDIVU, 0);
}

void genmfhi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[64]);
#endif
#ifdef INTERPRET_MFHI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MFHI, 0);
#else
    int rd = allocate_register_64_w((unsigned long long *) r4300->recomp.dst->f.r.rd);
    int _hi = allocate_register_64((unsigned long long *) r4300_mult_hi(r4300));

    mov_reg64_reg64(rd, _hi);
#endif
}

void genmthi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[65]);
#endif
#ifdef INTERPRET_MTHI
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MTHI, 0);
#else
    int _hi = allocate_register_64_w((unsigned long long *) r4300_mult_hi(r4300));
    int rs = allocate_register_64((unsigned long long *) r4300->recomp.dst->f.r.rs);

    mov_reg64_reg64(_hi, rs);
#endif
}

void genmflo(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[66]);
#endif
#ifdef INTERPRET_MFLO
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MFLO, 0);
#else
    int rd = allocate_register_64_w((unsigned long long *) r4300->recomp.dst->f.r.rd);
    int _lo = allocate_register_64((unsigned long long *) r4300_mult_lo(r4300));

    mov_reg64_reg64(rd, _lo);
#endif
}

void genmtlo(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[67]);
#endif
#ifdef INTERPRET_MTLO
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MTLO, 0);
#else
    int _lo = allocate_register_64_w((unsigned long long *)r4300_mult_lo(r4300));
    int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.r.rs);

    mov_reg64_reg64(_lo, rs);
#endif
}

/* Jump & Branch instructions */

static void gentest(struct r4300_core* r4300)
{
    cmp_m32rel_imm32((unsigned int *)(&r4300->branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt(r4300, (unsigned long long) (r4300->recomp.dst + (r4300->recomp.dst-1)->f.i.immediate));
    jmp(r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + 4);
    gencheck_interrupt(r4300, (unsigned long long)(r4300->recomp.dst + 1));
    jmp(r4300->recomp.dst->addr + 4);
}

static void gentest_out(struct r4300_core* r4300)
{
    cmp_m32rel_imm32((unsigned int *)(&r4300->branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(r4300, r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    mov_m32rel_imm32(&r4300->recomp.jump_to_address, r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);
    jump_end_rel32();

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + 4);
    gencheck_interrupt(r4300, (unsigned long long) (r4300->recomp.dst + 1));
    jmp(r4300->recomp.dst->addr + 4);
}

static void gentest_idle(struct r4300_core* r4300)
{
    int reg;

    reg = lru_register();
    free_register(reg);

    cmp_m32rel_imm32((unsigned int *)(&r4300->branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    mov_xreg32_m32rel(reg, (unsigned int *)(r4300_cp0_next_interrupt(&r4300->cp0)));
    sub_xreg32_m32rel(reg, (unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(reg, 3);
    jbe_rj(0);
    jump_start_rel8();

    and_reg32_imm32(reg, 0xFFFFFFFC);
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]), reg);

    jump_end_rel8();
    jump_end_rel32();
}

static void gentestl(struct r4300_core* r4300)
{
    cmp_m32rel_imm32((unsigned int *)(&r4300->branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    gendelayslot(r4300);
    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt(r4300, (unsigned long long) (r4300->recomp.dst + (r4300->recomp.dst-1)->f.i.immediate));
    jmp(r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);

    jump_end_rel32();

    gencp0_update_count(r4300, r4300->recomp.dst->addr-4);
    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + 4);
    gencheck_interrupt(r4300, (unsigned long long) (r4300->recomp.dst + 1));
    jmp(r4300->recomp.dst->addr + 4);
}

static void gentestl_out(struct r4300_core* r4300)
{
    cmp_m32rel_imm32((unsigned int *)(&r4300->branch_taken), 0);
    je_near_rj(0);
    jump_start_rel32();

    gendelayslot(r4300);
    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    gencheck_interrupt_out(r4300, r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);
    mov_m32rel_imm32(&r4300->recomp.jump_to_address, r4300->recomp.dst->addr + (r4300->recomp.dst-1)->f.i.immediate*4);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);

    jump_end_rel32();

    gencp0_update_count(r4300, r4300->recomp.dst->addr-4);
    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), r4300->recomp.dst->addr + 4);
    gencheck_interrupt(r4300, (unsigned long long) (r4300->recomp.dst + 1));
    jmp(r4300->recomp.dst->addr + 4);
}

static void genbranchlink(struct r4300_core* r4300)
{
    int r31_64bit = is64((unsigned int*)&r4300_regs(r4300)[31]);

    if (r31_64bit == 0)
    {
        int r31 = allocate_register_32_w((unsigned int *)&r4300_regs(r4300)[31]);

        mov_reg32_imm32(r31, r4300->recomp.dst->addr+8);
    }
    else if (r31_64bit == -1)
    {
        mov_m32rel_imm32((unsigned int *)&r4300_regs(r4300)[31], r4300->recomp.dst->addr + 8);
        if (r4300->recomp.dst->addr & 0x80000000) {
            mov_m32rel_imm32(((unsigned int *)&r4300_regs(r4300)[31])+1, 0xFFFFFFFF);
        }
        else {
            mov_m32rel_imm32(((unsigned int *)&r4300_regs(r4300)[31])+1, 0);
        }
    }
    else
    {
        int r31 = allocate_register_64_w((unsigned long long *)&r4300_regs(r4300)[31]);

        mov_reg32_imm32(r31, r4300->recomp.dst->addr+8);
        movsxd_reg64_reg32(r31, r31);
    }
}

void genj(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J, 1);
#else
    unsigned int naddr;

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J, 1);
        return;
    }

    gendelayslot(r4300);
    naddr = ((r4300->recomp.dst-1)->f.j.inst_index<<2) | (r4300->recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), naddr);
    gencheck_interrupt(r4300, (unsigned long long) &r4300->cached_interp.actual->block[(naddr-r4300->cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genj_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J_OUT, 1);
#else
    unsigned int naddr;

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J_OUT, 1);
        return;
    }

    gendelayslot(r4300);
    naddr = ((r4300->recomp.dst-1)->f.j.inst_index<<2) | (r4300->recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), naddr);
    gencheck_interrupt_out(r4300, naddr);
    mov_m32rel_imm32(&r4300->recomp.jump_to_address, naddr);
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long)dynarec_jump_to_address);
    call_reg64(RAX);
#endif
}

void genj_idle(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[2]);
#endif
#ifdef INTERPRET_J_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.J_IDLE, 1);
        return;
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_cp0_next_interrupt(&r4300->cp0)));
    sub_xreg32_m32rel(EAX, (unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(12);

    and_eax_imm32(0xFFFFFFFC);  // 5
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]), EAX); // 7

    genj(r4300);
#endif
}

void genjal(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL, 1);
#else
    unsigned int naddr;

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL, 1);
        return;
    }

    gendelayslot(r4300);

    mov_m32rel_imm32((unsigned int *)(r4300_regs(r4300) + 31), r4300->recomp.dst->addr + 4);
    if (((r4300->recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs(r4300)[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs(r4300)[31])+1, 0);
    }

    naddr = ((r4300->recomp.dst-1)->f.j.inst_index<<2) | (r4300->recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), naddr);
    gencheck_interrupt(r4300, (unsigned long long) &r4300->cached_interp.actual->block[(naddr-r4300->cached_interp.actual->start)/4]);
    jmp(naddr);
#endif
}

void genjal_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL_OUT, 1);
#else
    unsigned int naddr;

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL_OUT, 1);
        return;
    }

    gendelayslot(r4300);

    mov_m32rel_imm32((unsigned int *)(r4300_regs(r4300) + 31), r4300->recomp.dst->addr + 4);
    if (((r4300->recomp.dst->addr + 4) & 0x80000000)) {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs(r4300)[31])+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32((unsigned int *)(&r4300_regs(r4300)[31])+1, 0);
    }

    naddr = ((r4300->recomp.dst-1)->f.j.inst_index<<2) | (r4300->recomp.dst->addr & 0xF0000000);

    mov_m32rel_imm32((void*)(&r4300->cp0.last_addr), naddr);
    gencheck_interrupt_out(r4300, naddr);
    mov_m32rel_imm32(&r4300->recomp.jump_to_address, naddr);
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);
#endif
}

void genjal_idle(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[3]);
#endif
#ifdef INTERPRET_JAL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JAL_IDLE, 1);
        return;
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_cp0_next_interrupt(&r4300->cp0)));
    sub_xreg32_m32rel(EAX, (unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]));
    cmp_reg32_imm8(EAX, 3);
    jbe_rj(12);

    and_eax_imm32(0xFFFFFFFC);  // 5
    add_m32rel_xreg32((unsigned int *)(&r4300_cp0_regs(&r4300->cp0)[CP0_COUNT_REG]), EAX); // 7

    genjal(r4300);
#endif
}

void genjr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[61]);
#endif
#ifdef INTERPRET_JR
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JR, 1);
#else
    unsigned int diff = (unsigned int) offsetof(struct precomp_instr, local_addr);
    unsigned int diff_need = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.need_map);
    unsigned int diff_wrap = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.jump_wrapper);

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JR, 1);
        return;
    }

    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.i.rs);
    mov_m32rel_xreg32((unsigned int *)&r4300->local_rs, EAX);

    gendelayslot(r4300);

    mov_xreg32_m32rel(EAX, (unsigned int *)&r4300->local_rs);
    mov_m32rel_xreg32((unsigned int *)&r4300->cp0.last_addr, EAX);

    gencheck_interrupt_reg(r4300);

    mov_xreg32_m32rel(EAX, (unsigned int *)&r4300->local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(r4300->recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32rel_xreg32(&r4300->recomp.jump_to_address, EBX);
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);  /* will never return from call */

    jump_end_rel32();

    mov_reg64_imm64(RSI, (unsigned long long) r4300->recomp.dst_block->block);
    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(r4300->recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32rel((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(11);

    add_reg32_imm32(EAX, diff_wrap); // 6
    add_reg64_reg64(RAX, RSI); // 3
    jmp_reg64(RAX); // 2

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff);
    mov_rax_memoffs64((unsigned long long *) &r4300->recomp.dst_block->code);
    add_reg64_reg64(RAX, RBX);
    jmp_reg64(RAX);
#endif
}

void genjalr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[62]);
#endif
#ifdef INTERPRET_JALR
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JALR, 0);
#else
    unsigned int diff = (unsigned int) offsetof(struct precomp_instr, local_addr);
    unsigned int diff_need = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.need_map);
    unsigned int diff_wrap = (unsigned int) offsetof(struct precomp_instr, reg_cache_infos.jump_wrapper);

    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.JALR, 1);
        return;
    }

    free_registers_move_start();

    mov_xreg32_m32rel(EAX, (unsigned int *)r4300->recomp.dst->f.r.rs);
    mov_m32rel_xreg32((unsigned int *)&r4300->local_rs, EAX);

    gendelayslot(r4300);

    mov_m32rel_imm32((unsigned int *)(r4300->recomp.dst-1)->f.r.rd, r4300->recomp.dst->addr+4);
    if ((r4300->recomp.dst->addr+4) & 0x80000000) {
        mov_m32rel_imm32(((unsigned int *)(r4300->recomp.dst-1)->f.r.rd)+1, 0xFFFFFFFF);
    }
    else {
        mov_m32rel_imm32(((unsigned int *)(r4300->recomp.dst-1)->f.r.rd)+1, 0);
    }

    mov_xreg32_m32rel(EAX, (unsigned int *)&r4300->local_rs);
    mov_m32rel_xreg32((unsigned int *)&r4300->cp0.last_addr, EAX);

    gencheck_interrupt_reg(r4300);

    mov_xreg32_m32rel(EAX, (unsigned int *)&r4300->local_rs);
    mov_reg32_reg32(EBX, EAX);
    and_eax_imm32(0xFFFFF000);
    cmp_eax_imm32(r4300->recomp.dst_block->start & 0xFFFFF000);
    je_near_rj(0);

    jump_start_rel32();

    mov_m32rel_xreg32(&r4300->recomp.jump_to_address, EBX);
    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1));
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX);
    mov_reg64_imm64(RAX, (unsigned long long) dynarec_jump_to_address);
    call_reg64(RAX);  /* will never return from call */

    jump_end_rel32();

    mov_reg64_imm64(RSI, (unsigned long long) r4300->recomp.dst_block->block);
    mov_reg32_reg32(EAX, EBX);
    sub_eax_imm32(r4300->recomp.dst_block->start);
    shr_reg32_imm8(EAX, 2);
    mul_m32rel((unsigned int *)(&precomp_instr_size));

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff_need);
    cmp_reg32_imm32(EBX, 1);
    jne_rj(11);

    add_reg32_imm32(EAX, diff_wrap); // 6
    add_reg64_reg64(RAX, RSI); // 3
    jmp_reg64(RAX); // 2

    mov_reg32_preg64preg64pimm32(EBX, RAX, RSI, diff);
    mov_rax_memoffs64((unsigned long long *) &r4300->recomp.dst_block->code);
    add_reg64_reg64(RAX, RBX);
    jmp_reg64(RAX);
#endif
}

static void genbeq_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);
    int rt_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rt);

    if (rs_64bit == 0 && rt_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);
        int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        sete_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rs_64bit == -1)
    {
        int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rt);

        cmp_xreg64_m64rel(rt, (unsigned long long *) r4300->recomp.dst->f.i.rs);
        sete_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rt_64bit == -1)
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);

        cmp_xreg64_m64rel(rs, (unsigned long long *)r4300->recomp.dst->f.i.rt);
        sete_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
        int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rt);
        cmp_reg64_reg64(rs, rt);
        sete_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genbeq(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[4]);
#endif
#ifdef INTERPRET_BEQ
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ, 1);
        return;
    }

    genbeq_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbeq_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[4]);
#endif
#ifdef INTERPRET_BEQ_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ_OUT, 1);
        return;
    }

    genbeq_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbeq_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BEQ_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQ_IDLE, 1);
        return;
    }

    genbeq_test(r4300);
    gentest_idle(r4300);
    genbeq(r4300);
#endif
}

void genbeql(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[16]);
#endif
#ifdef INTERPRET_BEQL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL, 1);
        return;
    }

    genbeq_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbeql_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[16]);
#endif
#ifdef INTERPRET_BEQL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL_OUT, 1);
        return;
    }

    genbeq_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbeql_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BEQL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BEQL_IDLE, 1);
        return;
    }

    genbeq_test(r4300);
    gentest_idle(r4300);
    genbeql(r4300);
#endif
}

static void genbne_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);
    int rt_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rt);

    if (rs_64bit == 0 && rt_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);
        int rt = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rt);

        cmp_reg32_reg32(rs, rt);
        setne_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rs_64bit == -1)
    {
        int rt = allocate_register_64((unsigned long long *) r4300->recomp.dst->f.i.rt);

        cmp_xreg64_m64rel(rt, (unsigned long long *)r4300->recomp.dst->f.i.rs);
        setne_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rt_64bit == -1)
    {
        int rs = allocate_register_64((unsigned long long *) r4300->recomp.dst->f.i.rs);

        cmp_xreg64_m64rel(rs, (unsigned long long *)r4300->recomp.dst->f.i.rt);
        setne_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
        int rt = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rt);

        cmp_reg64_reg64(rs, rt);
        setne_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genbne(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[5]);
#endif
#ifdef INTERPRET_BNE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE, 1);
        return;
    }

    genbne_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbne_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[5]);
#endif
#ifdef INTERPRET_BNE_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE_OUT, 1);
        return;
    }

    genbne_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbne_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BNE_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNE_IDLE, 1);
        return;
    }

    genbne_test(r4300);
    gentest_idle(r4300);
    genbne(r4300);
#endif
}

void genbnel(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[17]);
#endif
#ifdef INTERPRET_BNEL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL, 1);
        return;
    }

    genbne_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbnel_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[17]);
#endif
#ifdef INTERPRET_BNEL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL_OUT, 1);
        return;
    }

    genbne_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbnel_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BNEL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BNEL_IDLE, 1);
        return;
    }

    genbne_test(r4300);
    gentest_idle(r4300);
    genbnel(r4300);
#endif
}

static void genblez_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setle_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setle_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genblez(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[6]);
#endif
#ifdef INTERPRET_BLEZ
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ, 1);
        return;
    }

    genblez_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genblez_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[6]);
#endif
#ifdef INTERPRET_BLEZ_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ_OUT, 1);
        return;
    }

    genblez_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genblez_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLEZ_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZ_IDLE, 1);
        return;
    }

    genblez_test(r4300);
    gentest_idle(r4300);
    genblez(r4300);
#endif
}

void genblezl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[18]);
#endif
#ifdef INTERPRET_BLEZL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL, 1);
        return;
    }

    genblez_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genblezl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[18]);
#endif
#ifdef INTERPRET_BLEZL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL_OUT, 1);
        return;
    }

    genblez_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genblezl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLEZL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLEZL_IDLE, 1);
        return;
    }

    genblez_test(r4300);
    gentest_idle(r4300);
    genblezl(r4300);
#endif
}

static void genbgtz_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setg_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setg_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genbgtz(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[7]);
#endif
#ifdef INTERPRET_BGTZ
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ, 1);
        return;
    }

    genbgtz_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbgtz_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[7]);
#endif
#ifdef INTERPRET_BGTZ_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ_OUT, 1);
        return;
    }

    genbgtz_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbgtz_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGTZ_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZ_IDLE, 1);
        return;
    }

    genbgtz_test(r4300);
    gentest_idle(r4300);
    genbgtz(r4300);
#endif
}

void genbgtzl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[19]);
#endif
#ifdef INTERPRET_BGTZL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL, 1);
        return;
    }

    genbgtz_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbgtzl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[19]);
#endif
#ifdef INTERPRET_BGTZL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL_OUT, 1);
        return;
    }

    genbgtz_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbgtzl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGTZL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGTZL_IDLE, 1);
        return;
    }

    genbgtz_test(r4300);
    gentest_idle(r4300);
    genbgtzl(r4300);
#endif
}

static void genbltz_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);

        cmp_reg32_imm32(rs, 0);
        setl_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rs_64bit == -1)
    {
        cmp_m32rel_imm32(((unsigned int *)r4300->recomp.dst->f.i.rs)+1, 0);
        setl_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);

        cmp_reg64_imm8(rs, 0);
        setl_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genbltz(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[47]);
#endif
#ifdef INTERPRET_BLTZ
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ, 1);
        return;
    }

    genbltz_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbltz_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[47]);
#endif
#ifdef INTERPRET_BLTZ_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ_OUT, 1);
        return;
    }

    genbltz_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbltz_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLTZ_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZ_IDLE, 1);
        return;
    }

    genbltz_test(r4300);
    gentest_idle(r4300);
    genbltz(r4300);
#endif
}

void genbltzal(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[51]);
#endif
#ifdef INTERPRET_BLTZAL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbltzal_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[51]);
#endif
#ifdef INTERPRET_BLTZAL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL_OUT, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbltzal_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLTZAL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZAL_IDLE, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    gentest_idle(r4300);
    genbltzal(r4300);
#endif
}

void genbltzl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[49]);
#endif
#ifdef INTERPRET_BLTZL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL, 1);
        return;
    }

    genbltz_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbltzl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[49]);
#endif
#ifdef INTERPRET_BLTZL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL_OUT, 1);
        return;
    }

    genbltz_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbltzl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLTZL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZL_IDLE, 1);
        return;
    }

    genbltz_test(r4300);
    gentest_idle(r4300);
    genbltzl(r4300);
#endif
}

void genbltzall(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[53]);
#endif
#ifdef INTERPRET_BLTZALL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbltzall_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[53]);
#endif
#ifdef INTERPRET_BLTZALL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL_OUT, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbltzall_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BLTZALL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BLTZALL_IDLE, 1);
        return;
    }

    genbltz_test(r4300);
    genbranchlink(r4300);
    gentest_idle(r4300);
    genbltzall(r4300);
#endif
}

static void genbgez_test(struct r4300_core* r4300)
{
    int rs_64bit = is64((unsigned int *)r4300->recomp.dst->f.i.rs);

    if (rs_64bit == 0)
    {
        int rs = allocate_register_32((unsigned int *)r4300->recomp.dst->f.i.rs);
        cmp_reg32_imm32(rs, 0);
        setge_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else if (rs_64bit == -1)
    {
        cmp_m32rel_imm32(((unsigned int *)r4300->recomp.dst->f.i.rs)+1, 0);
        setge_m8rel((unsigned char *) &r4300->branch_taken);
    }
    else
    {
        int rs = allocate_register_64((unsigned long long *)r4300->recomp.dst->f.i.rs);
        cmp_reg64_imm8(rs, 0);
        setge_m8rel((unsigned char *) &r4300->branch_taken);
    }
}

void genbgez(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[48]);
#endif
#ifdef INTERPRET_BGEZ
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ, 1);
        return;
    }

    genbgez_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbgez_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[48]);
#endif
#ifdef INTERPRET_BGEZ_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ_OUT, 1);
        return;
    }

    genbgez_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbgez_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGEZ_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZ_IDLE, 1);
        return;
    }

    genbgez_test(r4300);
    gentest_idle(r4300);
    genbgez(r4300);
#endif
}

void genbgezal(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[52]);
#endif
#ifdef INTERPRET_BGEZAL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbgezal_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[52]);
#endif
#ifdef INTERPRET_BGEZAL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL_OUT, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbgezal_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGEZAL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZAL_IDLE, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    gentest_idle(r4300);
    genbgezal(r4300);
#endif
}

void genbgezl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[50]);
#endif
#ifdef INTERPRET_BGEZL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL, 1);
        return;
    }

    genbgez_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbgezl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[50]);
#endif
#ifdef INTERPRET_BGEZL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL_OUT, 1);
        return;
    }

    genbgez_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbgezl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGEZL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZL_IDLE, 1);
        return;
    }

    genbgez_test(r4300);
    gentest_idle(r4300);
    genbgezl(r4300);
#endif
}

void genbgezall(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[54]);
#endif
#ifdef INTERPRET_BGEZALL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbgezall_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[54]);
#endif
#ifdef INTERPRET_BGEZALL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL_OUT, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbgezall_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BGEZALL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BGEZALL_IDLE, 1);
        return;
    }

    genbgez_test(r4300);
    genbranchlink(r4300);
    gentest_idle(r4300);
    genbgezall(r4300);
#endif
}





/* global functions */


static void genbc1f_test(struct r4300_core* r4300)
{
    test_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000);
    sete_m8rel((unsigned char *) &r4300->branch_taken);
}

void genbc1f(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[100]);
#endif
#ifdef INTERPRET_BC1F
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC &&
                (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))||r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbc1f_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[100]);
#endif
#ifdef INTERPRET_BC1F_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC &&
                (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))||r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F_OUT, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbc1f_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BC1F_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC &&
                (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))||r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1F_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    gentest_idle(r4300);
    genbc1f(r4300);
#endif
}

void genbc1fl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[102]);
#endif
#ifdef INTERPRET_BC1FL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbc1fl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[102]);
#endif
#ifdef INTERPRET_BC1FL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL_OUT, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbc1fl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BC1FL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1FL_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1f_test(r4300);
    gentest_idle(r4300);
    genbc1fl(r4300);
#endif
}

static void genbc1t_test(struct r4300_core* r4300)
{
    test_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000);
    setne_m8rel((unsigned char *) &r4300->branch_taken);
}

void genbc1t(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[101]);
#endif
#ifdef INTERPRET_BC1T
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    gendelayslot(r4300);
    gentest(r4300);
#endif
}

void genbc1t_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[101]);
#endif
#ifdef INTERPRET_BC1T_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T_OUT, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    gendelayslot(r4300);
    gentest_out(r4300);
#endif
}

void genbc1t_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BC1T_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1T_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    gentest_idle(r4300);
    genbc1t(r4300);
#endif
}

void genbc1tl(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[103]);
#endif
#ifdef INTERPRET_BC1TL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    free_all_registers();
    gentestl(r4300);
#endif
}

void genbc1tl_out(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[103]);
#endif
#ifdef INTERPRET_BC1TL_OUT
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL_OUT, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL_OUT, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    free_all_registers();
    gentestl_out(r4300);
#endif
}

void genbc1tl_idle(struct r4300_core* r4300)
{
#ifdef INTERPRET_BC1TL_IDLE
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL_IDLE, 1);
#else
    if (((r4300->recomp.dst->addr & 0xFFF) == 0xFFC
        && (r4300->recomp.dst->addr < 0x80000000 || r4300->recomp.dst->addr >= 0xC0000000))
        || r4300->recomp.no_compiled_jump)
    {
        gencallinterp(r4300, (unsigned long long)cached_interpreter_table.BC1TL_IDLE, 1);
        return;
    }

    gencheck_cop1_unusable(r4300);
    genbc1t_test(r4300);
    gentest_idle(r4300);
    genbc1tl(r4300);
#endif
}

/* Special instructions */

void gencache(struct r4300_core* r4300)
{
}

void generet(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[108]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ERET, 1);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(r4300))), (unsigned int)(dst));
    genupdate_system(r4300, 0);
    mov_reg32_imm32(EAX, (unsigned int)(ERET));
    call_reg32(EAX);
    mov_reg32_imm32(EAX, (unsigned int)(jump_code));
    jmp_reg32(EAX);
#endif
}

void gensync(struct r4300_core* r4300)
{
}

void gensyscall(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[63]);
#endif
#ifdef INTERPRET_SYSCALL
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SYSCALL, 0);
#else
    free_registers_move_start();

    mov_m32rel_imm32(&r4300_cp0_regs(&r4300->cp0)[CP0_CAUSE_REG], 8 << 2);
    gencallinterp(r4300, (unsigned long long)dynarec_exception_general, 0);
#endif
}

/* Exception instructions */

void genteq(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[96]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TEQ, 0);
}

/* TLB instructions */

void gentlbp(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[105]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TLBP, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBP));
    call_reg32(EAX);
    genupdate_system(r4300, 0);
#endif
}

void gentlbr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[106]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TLBR, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBR));
    call_reg32(EAX);
    genupdate_system(r4300, 0);
#endif
}

void gentlbwr(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[107]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TLBWR, 0);
}

void gentlbwi(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[104]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TLBWI, 0);
#if 0
    dst->local_addr = code_length;
    mov_m32_imm32((void *)(&(*r4300_pc_struct(r4300))), (unsigned int)(dst));
    mov_reg32_imm32(EAX, (unsigned int)(TLBWI));
    call_reg32(EAX);
    genupdate_system(r4300, 0);
#endif
}

/* CP0 load/store instructions */

void genmfc0(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[109]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MFC0, 0);
}

void genmtc0(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[110]);
#endif
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MTC0, 0);
}

/* CP1 load/store instructions */

void genlwc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[39]);
#endif
#ifdef INTERPRET_LWC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LWC1, 0);
#else
    gencheck_cop1_unusable(r4300);

    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs(r4300)[r4300->recomp.dst->f.lf.base]));
    add_eax_imm32((int)r4300->recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(52);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.lf.ft])); // 7
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), RDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_read_aligned_word); // 10
    call_reg64(RBX); // 2
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg32_preg64preg64(EAX, RBX, RSI); // 3
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.lf.ft])); // 7
    mov_preg64_reg32(RBX, EAX); // 2
#endif
}

void genldc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[40]);
#endif
#ifdef INTERPRET_LDC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.LDC1, 0);
#else
    gencheck_cop1_unusable(r4300);

    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs(r4300)[r4300->recomp.dst->f.lf.base]));
    add_eax_imm32((int)r4300->recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].read32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) read_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(52);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.lf.ft])); // 7
    mov_m64rel_xreg64((unsigned long long *)(&r4300->rdword), RDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_read_aligned_dword); // 10
    call_reg64(RBX); // 2
    jmp_imm_short(39); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_reg64_preg64preg64(RAX, RBX, RSI); // 4
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.lf.ft])); // 7
    mov_preg64pimm32_reg32(RBX, 4, EAX); // 6
    shr_reg64_imm8(RAX, 32); // 4
    mov_preg64_reg32(RBX, EAX); // 2
#endif
}

void genswc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[43]);
#endif
#ifdef INTERPRET_SWC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SWC1, 0);
#else
    gencheck_cop1_unusable(r4300);

    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.lf.ft]));
    mov_reg32_preg64(ECX, RDX);
    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs(r4300)[r4300->recomp.dst->f.lf.base]));
    add_eax_imm32((int)r4300->recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(63);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wword(r4300)), ECX); // 7
    mov_m32rel_imm32((unsigned int *)(r4300_wmask(r4300)), ~UINT32_C(0)); // 11
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_word); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(21); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64_reg32(RBX, RSI, ECX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void gensdc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[44]);
#endif
#ifdef INTERPRET_SDC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SDC1, 0);
#else
    gencheck_cop1_unusable(r4300);

    mov_xreg64_m64rel(RSI, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.lf.ft]));
    mov_reg32_preg64(ECX, RSI);
    mov_reg32_preg64pimm32(EDX, RSI, 4);
    mov_xreg32_m32rel(EAX, (unsigned int *)(&r4300_regs(r4300)[r4300->recomp.dst->f.lf.base]));
    add_eax_imm32((int)r4300->recomp.dst->f.lf.offset);
    mov_reg32_reg32(EBX, EAX);

    /* is address in RDRAM ? */
    and_reg32_imm32(EAX, 0xDF800000);
    cmp_reg32_imm32(EAX, 0x80000000);

    /* when fast_memory is true, we know that there is
     * no custom read handler so skip this test entirely */
    if (!r4300->recomp.fast_memory) {
        /* not in RDRAM anyway so skip the read32 check */
        jne_rj(0);
        jump_start_rel8();

        shr_reg64_imm8(RAX, 16);
        and_reg32_imm32(EAX, 0x1fff);
        lea_reg64_preg64x2preg64(RAX, RAX, RAX);
        mov_reg64_imm64(RSI, (unsigned long long) r4300->mem->handlers[0].write32);
        mov_reg64_preg64x8preg64(RAX, RAX, RSI);
        mov_reg64_imm64(RSI, (unsigned long long) write_rdram_dram);
        cmp_reg64_reg64(RAX, RSI);

        jump_end_rel8();
    }
    je_rj(59);

    mov_reg64_imm64(RAX, (unsigned long long) (r4300->recomp.dst+1)); // 10
    mov_m64rel_xreg64((unsigned long long *)(&(*r4300_pc_struct(r4300))), RAX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_address(r4300)), EBX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword(r4300)), ECX); // 7
    mov_m32rel_xreg32((unsigned int *)(r4300_wdword(r4300))+1, EDX); // 7
    mov_reg64_imm64(RBX, (unsigned long long)dynarec_write_aligned_dword); // 10
    call_reg64(RBX); // 2
    mov_xreg32_m32rel(EAX, (unsigned int *)(r4300_address(r4300))); // 7
    jmp_imm_short(28); // 2

    mov_reg64_imm64(RSI, (unsigned long long) r4300->rdram->dram); // 10
    mov_reg32_reg32(EAX, EBX); // 2
    and_reg32_imm32(EBX, 0x7FFFFF); // 6
    mov_preg64preg64pimm32_reg32(RBX, RSI, 4, ECX); // 7
    mov_preg64preg64_reg32(RBX, RSI, EDX); // 3

    mov_reg64_imm64(RSI, (unsigned long long) r4300->cached_interp.invalid_code);
    mov_reg32_reg32(EBX, EAX);
    shr_reg32_imm8(EBX, 12);
    cmp_preg64preg64_imm8(RBX, RSI, 0);
    jne_rj(65);

    mov_reg64_imm64(RDI, (unsigned long long) r4300->cached_interp.blocks); // 10
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

void genmfc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[111]);
#endif
#ifdef INTERPRET_MFC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MFC1, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.r.nrd]));
    mov_reg32_preg64(EBX, RAX);
    mov_m32rel_xreg32((unsigned int*)r4300->recomp.dst->f.r.rt, EBX);
    sar_reg32_imm8(EBX, 31);
    mov_m32rel_xreg32(((unsigned int*)r4300->recomp.dst->f.r.rt)+1, EBX);
#endif
}

void gendmfc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[112]);
#endif
#ifdef INTERPRET_DMFC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DMFC1, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *) (&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.r.nrd]));
    mov_reg32_preg64(EBX, RAX);
    mov_reg32_preg64pimm32(ECX, RAX, 4);
    mov_m32rel_xreg32((unsigned int*)r4300->recomp.dst->f.r.rt, EBX);
    mov_m32rel_xreg32(((unsigned int*)r4300->recomp.dst->f.r.rt)+1, ECX);
#endif
}

void gencfc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[113]);
#endif
#ifdef INTERPRET_CFC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CFC1, 0);
#else
    gencheck_cop1_unusable(r4300);
    if (r4300->recomp.dst->f.r.nrd == 31) {
        mov_xreg32_m32rel(EAX, (unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)));
    }
    else {
        mov_xreg32_m32rel(EAX, (unsigned int*)&(*r4300_cp1_fcr0(&r4300->cp1)));
    }
    mov_m32rel_xreg32((unsigned int*)r4300->recomp.dst->f.r.rt, EAX);
    sar_reg32_imm8(EAX, 31);
    mov_m32rel_xreg32(((unsigned int*)r4300->recomp.dst->f.r.rt)+1, EAX);
#endif
}

void genmtc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[114]);
#endif
#ifdef INTERPRET_MTC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MTC1, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg32_m32rel(EAX, (unsigned int*)r4300->recomp.dst->f.r.rt);
    mov_xreg64_m64rel(RBX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.r.nrd]));
    mov_preg64_reg32(RBX, EAX);
#endif
}

void gendmtc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[115]);
#endif
#ifdef INTERPRET_DMTC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DMTC1, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg32_m32rel(EAX, (unsigned int*)r4300->recomp.dst->f.r.rt);
    mov_xreg32_m32rel(EBX, ((unsigned int*)r4300->recomp.dst->f.r.rt)+1);
    mov_xreg64_m64rel(RDX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.r.nrd]));
    mov_preg64_reg32(RDX, EAX);
    mov_preg64pimm32_reg32(RDX, 4, EBX);
#endif
}

void genctc1(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[116]);
#endif
#ifdef INTERPRET_CTC1
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CTC1, 0);
#else
    gencheck_cop1_unusable(r4300);

    if (r4300->recomp.dst->f.r.nrd != 31) {
        return;
    }
    mov_xreg32_m32rel(EAX, (unsigned int*)r4300->recomp.dst->f.r.rt);
    mov_m32rel_xreg32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), EAX);
    and_eax_imm32(3);

    cmp_eax_imm32(0);
    jne_rj(13);
    mov_m32rel_imm32((unsigned int*)&r4300->cp1.rounding_mode, 0x33F); // 11
    jmp_imm_short(51); // 2

    cmp_eax_imm32(1); // 5
    jne_rj(13); // 2
    mov_m32rel_imm32((unsigned int*)&r4300->cp1.rounding_mode, 0xF3F); // 11
    jmp_imm_short(31); // 2

    cmp_eax_imm32(2); // 5
    jne_rj(13); // 2
    mov_m32rel_imm32((unsigned int*)&r4300->cp1.rounding_mode, 0xB3F); // 11
    jmp_imm_short(11); // 2

    mov_m32rel_imm32((unsigned int*)&r4300->cp1.rounding_mode, 0x73F); // 11

    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

/* CP1 computational instructions */

void genabs_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[124]);
#endif
#ifdef INTERPRET_ABS_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ABS_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fabs_();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genabs_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[124]);
#endif
#ifdef INTERPRET_ABS_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ABS_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fabs_();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genadd_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[119]);
#endif
#ifdef INTERPRET_ADD_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADD_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fadd_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genadd_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[119]);
#endif
#ifdef INTERPRET_ADD_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ADD_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fadd_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gendiv_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[122]);
#endif
#ifdef INTERPRET_DIV_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DIV_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fdiv_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gendiv_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[122]);
#endif
#ifdef INTERPRET_DIV_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.DIV_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fdiv_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genmov_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[125]);
#endif
#ifdef INTERPRET_MOV_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MOV_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    mov_reg32_preg64(EBX, RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    mov_preg64_reg32(RAX, EBX);
#endif
}

void genmov_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[125]);
#endif
#ifdef INTERPRET_MOV_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MOV_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    mov_reg32_preg64(EBX, RAX);
    mov_reg32_preg64pimm32(ECX, RAX, 4);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    mov_preg64_reg32(RAX, EBX);
    mov_preg64pimm32_reg32(RAX, 4, ECX);
#endif
}

void genmul_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[121]);
#endif
#ifdef INTERPRET_MUL_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MUL_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fmul_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genmul_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[121]);
#endif
#ifdef INTERPRET_MUL_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.MUL_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fmul_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void genneg_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[126]);
#endif
#ifdef INTERPRET_NEG_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.NEG_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fchs();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void genneg_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[126]);
#endif
#ifdef INTERPRET_NEG_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.NEG_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fchs();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gensqrt_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[123]);
#endif
#ifdef INTERPRET_SQRT_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SQRT_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fsqrt();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gensqrt_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[123]);
#endif
#ifdef INTERPRET_SQRT_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SQRT_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fsqrt();
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gensub_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[120]);
#endif
#ifdef INTERPRET_SUB_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SUB_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fsub_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gensub_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[120]);
#endif
#ifdef INTERPRET_SUB_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.SUB_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fsub_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gentrunc_w_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_W_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TRUNC_W_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void gentrunc_w_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_W_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TRUNC_W_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void gentrunc_l_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_L_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TRUNC_L_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void gentrunc_l_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[128]);
#endif
#ifdef INTERPRET_TRUNC_L_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.TRUNC_L_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&trunc_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genround_w_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_W_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ROUND_W_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genround_w_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_W_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ROUND_W_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genround_l_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_L_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ROUND_L_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genround_l_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[127]);
#endif
#ifdef INTERPRET_ROUND_L_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.ROUND_L_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&round_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genceil_w_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_W_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CEIL_W_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genceil_w_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_W_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CEIL_W_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genceil_l_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_L_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CEIL_L_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genceil_l_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[129]);
#endif
#ifdef INTERPRET_CEIL_L_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CEIL_L_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&ceil_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genfloor_w_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_W_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.FLOOR_W_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genfloor_w_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_W_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.FLOOR_W_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genfloor_l_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_L_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.FLOOR_L_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void genfloor_l_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[130]);
#endif
#ifdef INTERPRET_FLOOR_L_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.FLOOR_L_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    fldcw_m16rel((unsigned short*)&floor_mode);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
    fldcw_m16rel((unsigned short*)&r4300->cp1.rounding_mode);
#endif
}

void gencvt_s_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_S_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_s_w(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_W
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_S_W, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fild_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_s_l(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_L
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_S_L, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fild_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_dword(RAX);
#endif
}

void gencvt_d_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_D_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_d_w(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_W
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_D_W, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fild_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_d_l(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_L
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_D_L, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fild_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fstp_preg64_qword(RAX);
#endif
}

void gencvt_w_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_W_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_W_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
#endif
}

void gencvt_w_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_W_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_W_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_dword(RAX);
#endif
}

void gencvt_l_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_L_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_L_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
#endif
}

void gencvt_l_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_L_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.CVT_L_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fd]));
    fistp_preg64_qword(RAX);
#endif
}

/* CP1 relational instructions */

void genc_f_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_F_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_F_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000);
#endif
}

void genc_f_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_F_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_F_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000);
#endif
}

void genc_un_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UN_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_UN_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
#endif
}

void genc_un_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UN_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_UN_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
#endif
}

void genc_eq_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_EQ_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_EQ_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_eq_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_EQ_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_EQ_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ueq_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UEQ_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_UEQ_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ueq_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_UEQ_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_UEQ_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_olt_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLT_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_OLT_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_olt_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLT_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_OLT_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ult_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULT_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_ULT_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ult_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULT_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_ULT_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ole_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLE_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_OLE_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ole_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_OLE_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_OLE_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ule_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULE_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_ULE_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ule_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_ULE_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_ULE_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fucomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_sf_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SF_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_SF_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000);
#endif
}

void genc_sf_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SF_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_SF_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000);
#endif
}

void genc_ngle_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGLE_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGLE_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
#endif
}

void genc_ngle_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGLE_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGLE_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(13);
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
    jmp_imm_short(11); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
#endif
}

void genc_seq_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SEQ_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_SEQ_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_seq_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_SEQ_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_SEQ_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jne_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ngl_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGL_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGL_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ngl_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGL_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGL_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jne_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_lt_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LT_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_LT_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_lt_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LT_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_LT_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_nge_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGE_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGE_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_nge_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGE_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGE_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    jae_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_le_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LE_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_LE_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_le_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_LE_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_LE_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ngt_s(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGT_S
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGT_S, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_dword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_dword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13);
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}

void genc_ngt_d(struct r4300_core* r4300)
{
#if defined(COUNT_INSTR)
    inc_m32rel(&instr_count[118]);
#endif
#ifdef INTERPRET_C_NGT_D
    gencallinterp(r4300, (unsigned long long)cached_interpreter_table.C_NGT_D, 0);
#else
    gencheck_cop1_unusable(r4300);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.ft]));
    fld_preg64_qword(RAX);
    mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double(&r4300->cp1))[r4300->recomp.dst->f.cf.fs]));
    fld_preg64_qword(RAX);
    fcomip_fpreg(1);
    ffree_fpreg(0);
    jp_rj(15);
    ja_rj(13); // 2
    or_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), 0x800000); // 11
    jmp_imm_short(11); // 2
    and_m32rel_imm32((unsigned int*)&(*r4300_cp1_fcr31(&r4300->cp1)), ~0x800000); // 11
#endif
}
