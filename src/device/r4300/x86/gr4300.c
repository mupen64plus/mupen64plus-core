/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - gr4300.c                                                *
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

#include "api/debugger.h"
#include "assemble.h"
#include "interpret.h"
#include "regcache.h"
#include "device/memory/memory.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/cp1.h"
#include "device/r4300/exception.h"
#include "device/r4300/interrupt.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomph.h"
#include "main/main.h"

/* static functions */

static void gencp0_update_count(unsigned int addr)
{
#if !defined(COMPARE_CORE) && !defined(DBG)
   mov_reg32_imm32(EAX, addr);
   sub_reg32_m32(EAX, (unsigned int*)(&g_dev.r4300.cp0.last_addr));
   shr_reg32_imm8(EAX, 2);
   mov_reg32_m32(EDX, &g_dev.r4300.cp0.count_per_op);
   mul_reg32(EDX);
   add_m32_reg32((unsigned int*)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX);
#else
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1));
   mov_reg32_imm32(EAX, (unsigned int)cp0_update_count);
   call_reg32(EAX);
#endif
}

static void gencheck_interrupt(unsigned int instr_structure)
{
   mov_eax_memoffs32(r4300_cp0_next_interrupt());
   cmp_reg32_m32(EAX, &r4300_cp0_regs()[CP0_COUNT_REG]);
   ja_rj(17);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), instr_structure); // 10
   mov_reg32_imm32(EAX, (unsigned int)gen_interrupt); // 5
   call_reg32(EAX); // 2
}

static void gencheck_interrupt_out(unsigned int addr)
{
   mov_eax_memoffs32(r4300_cp0_next_interrupt());
   cmp_reg32_m32(EAX, &r4300_cp0_regs()[CP0_COUNT_REG]);
   ja_rj(27);
   mov_m32_imm32((unsigned int*)(&g_dev.r4300.fake_instr.addr), addr);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(&g_dev.r4300.fake_instr));
   mov_reg32_imm32(EAX, (unsigned int)gen_interrupt);
   call_reg32(EAX);
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


/* global functions */

void gennotcompiled(void)
{
    free_all_registers();
    simplify_access();

    mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst));
    mov_reg32_imm32(EAX, (unsigned int)cached_interpreter_table.NOTCOMPILED);
    call_reg32(EAX);
}

void genlink_subblock(void)
{
   free_all_registers();
   jmp(g_dev.r4300.recomp.dst->addr+4);
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
   
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst));
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

void gencallinterp(uintptr_t addr, int jump)
{
   free_all_registers();
   simplify_access();
   if (jump)
     mov_m32_imm32((unsigned int*)(&g_dev.r4300.dyna_interp), 1);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst));
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

void genni(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.NI, 0);
}

void genreserved(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.RESERVED, 0);
}

void genfin_block(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.FIN_BLOCK, 0);
}

void gencheck_interrupt_reg(void) // addr is in EAX
{
   mov_reg32_m32(EBX, r4300_cp0_next_interrupt());
   cmp_reg32_m32(EBX, &r4300_cp0_regs()[CP0_COUNT_REG]);
   ja_rj(22);
   mov_memoffs32_eax((unsigned int*)(&g_dev.r4300.fake_instr.addr)); // 5
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(&g_dev.r4300.fake_instr)); // 10
   mov_reg32_imm32(EAX, (unsigned int)gen_interrupt); // 5
   call_reg32(EAX); // 2
}

void gennop(void)
{
}

void genj(void)
{
#ifdef INTERPRET_J
   gencallinterp((unsigned int)cached_interpreter_table.J, 1);
#else
   unsigned int naddr;
   
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.J_OUT, 1);
    return;
     }
   
   gendelayslot();
   naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);
   
   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
   gencheck_interrupt_out(naddr);
   mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1));
   mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
   call_reg32(EAX);
#endif
}

void genj_idle(void)
{
#ifdef INTERPRET_J_IDLE
   gencallinterp((unsigned int)cached_interpreter_table.J_IDLE, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.J_IDLE, 1);
    return;
     }
   
   mov_eax_memoffs32((unsigned int *)(r4300_cp0_next_interrupt()));
   sub_reg32_m32(EAX, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
   cmp_reg32_imm8(EAX, 3);
   jbe_rj(11);
   
   and_eax_imm32(0xFFFFFFFC);  // 5
   add_m32_reg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX); // 6
  
   genj();
#endif
}

void genjal(void)
{
#ifdef INTERPRET_JAL
   gencallinterp((unsigned int)cached_interpreter_table.JAL, 1);
#else
   unsigned int naddr;
   
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.JAL, 1);
    return;
     }
   
   gendelayslot();
   
   mov_m32_imm32((unsigned int *)(r4300_regs() + 31), g_dev.r4300.recomp.dst->addr + 4);
   if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000))
     mov_m32_imm32((unsigned int *)(&r4300_regs()[31])+1, 0xFFFFFFFF);
   else
     mov_m32_imm32((unsigned int *)(&r4300_regs()[31])+1, 0);
   
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
   
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.JAL_OUT, 1);
    return;
     }
   
   gendelayslot();
   
   mov_m32_imm32((unsigned int *)(r4300_regs() + 31), g_dev.r4300.recomp.dst->addr + 4);
   if (((g_dev.r4300.recomp.dst->addr + 4) & 0x80000000))
     mov_m32_imm32((unsigned int *)(&r4300_regs()[31])+1, 0xFFFFFFFF);
   else
     mov_m32_imm32((unsigned int *)(&r4300_regs()[31])+1, 0);
   
   naddr = ((g_dev.r4300.recomp.dst-1)->f.j.inst_index<<2) | (g_dev.r4300.recomp.dst->addr & 0xF0000000);
   
   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, naddr);
   gencheck_interrupt_out(naddr);
   mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, naddr);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1));
   mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
   call_reg32(EAX);
#endif
}

void genjal_idle(void)
{
#ifdef INTERPRET_JAL_IDLE
   gencallinterp((unsigned int)cached_interpreter_table.JAL_IDLE, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.JAL_IDLE, 1);
    return;
     }
   
   mov_eax_memoffs32((unsigned int *)(r4300_cp0_next_interrupt()));
   sub_reg32_m32(EAX, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
   cmp_reg32_imm8(EAX, 3);
   jbe_rj(11);
   
   and_eax_imm32(0xFFFFFFFC);
   add_m32_reg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), EAX);
  
   genjal();
#endif
}

void gentest(void)
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

void genbeq(void)
{
#ifdef INTERPRET_BEQ
   gencallinterp((unsigned int)cached_interpreter_table.BEQ, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BEQ, 1);
    return;
     }
   
   genbeq_test();
   gendelayslot();
   gentest();
#endif
}

void gentest_out(void)
{
   cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
   je_near_rj(0);

   jump_start_rel32();

   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1));
   mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
   call_reg32(EAX);
   
   jump_end_rel32();

   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
   gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
   jmp(g_dev.r4300.recomp.dst->addr + 4);
}

void genbeq_out(void)
{
#ifdef INTERPRET_BEQ_OUT
   gencallinterp((unsigned int)cached_interpreter_table.BEQ_OUT, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BEQ_OUT, 1);
    return;
     }
   
   genbeq_test();
   gendelayslot();
   gentest_out();
#endif
}

void gentest_idle(void)
{
   int reg;
   
   reg = lru_register();
   free_register(reg);
   
   cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
   je_near_rj(0);

   jump_start_rel32();
   
   mov_reg32_m32(reg, (unsigned int *)(r4300_cp0_next_interrupt()));
   sub_reg32_m32(reg, (unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]));
   cmp_reg32_imm8(reg, 5);
   jbe_rj(18);
   
   sub_reg32_imm32(reg, 2); // 6
   and_reg32_imm32(reg, 0xFFFFFFFC); // 6
   add_m32_reg32((unsigned int *)(&r4300_cp0_regs()[CP0_COUNT_REG]), reg); // 6
   
   jump_end_rel32();
}

void genbeq_idle(void)
{
#ifdef INTERPRET_BEQ_IDLE
   gencallinterp((unsigned int)cached_interpreter_table.BEQ_IDLE, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BEQ_IDLE, 1);
    return;
     }
   
   genbeq_test();
   gentest_idle();
   genbeq();
#endif
}

void genbne(void)
{
#ifdef INTERPRET_BNE
   gencallinterp((unsigned int)cached_interpreter_table.BNE, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BNE_IDLE, 1);
    return;
     }
   
   genbne_test();
   gentest_idle();
   genbne();
#endif
}

void genblez(void)
{
#ifdef INTERPRET_BLEZ
   gencallinterp((unsigned int)cached_interpreter_table.BLEZ, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BLEZ_IDLE, 1);
    return;
     }
   
   genblez_test();
   gentest_idle();
   genblez();
#endif
}

void genbgtz(void)
{
#ifdef INTERPRET_BGTZ
   gencallinterp((unsigned int)cached_interpreter_table.BGTZ, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BGTZ_IDLE, 1);
    return;
     }
   
   genbgtz_test();
   gentest_idle();
   genbgtz();
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

void genlui(void)
{
#ifdef INTERPRET_LUI
   gencallinterp((unsigned int)cached_interpreter_table.LUI, 0);
#else
   int rt = allocate_register_w((unsigned int *)g_dev.r4300.recomp.dst->f.i.rt);
   
   mov_reg32_imm32(rt, (unsigned int)g_dev.r4300.recomp.dst->f.i.immediate << 16);
#endif
}

void gentestl(void)
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

void genbeql(void)
{
#ifdef INTERPRET_BEQL
   gencallinterp((unsigned int)cached_interpreter_table.BEQL, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BEQL, 1);
    return;
     }
   
   genbeq_test();
   free_all_registers();
   gentestl();
#endif
}

void gentestl_out(void)
{
   cmp_m32_imm32((unsigned int *)(&g_dev.r4300.branch_taken), 0);
   je_near_rj(0);

   jump_start_rel32();

   gendelayslot();
   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   gencheck_interrupt_out(g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   mov_m32_imm32(&g_dev.r4300.recomp.jump_to_address, g_dev.r4300.recomp.dst->addr + (g_dev.r4300.recomp.dst-1)->f.i.immediate*4);
   mov_m32_imm32((unsigned int*)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1));
   mov_reg32_imm32(EAX, (unsigned int)dynarec_jump_to_address);
   call_reg32(EAX);
   
   jump_end_rel32();

   gencp0_update_count(g_dev.r4300.recomp.dst->addr+4);
   mov_m32_imm32(&g_dev.r4300.cp0.last_addr, g_dev.r4300.recomp.dst->addr + 4);
   gencheck_interrupt((unsigned int)(g_dev.r4300.recomp.dst + 1));
   jmp(g_dev.r4300.recomp.dst->addr + 4);
}

void genbeql_out(void)
{
#ifdef INTERPRET_BEQL_OUT
   gencallinterp((unsigned int)cached_interpreter_table.BEQL_OUT, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BEQL_IDLE, 1);
    return;
     }
   
   genbeq_test();
   gentest_idle();
   genbeql();
#endif
}

void genbnel(void)
{
#ifdef INTERPRET_BNEL
   gencallinterp((unsigned int)cached_interpreter_table.BNEL, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BNEL_IDLE, 1);
    return;
     }
   
   genbne_test();
   gentest_idle();
   genbnel();
#endif
}

void genblezl(void)
{
#ifdef INTERPRET_BLEZL
   gencallinterp((unsigned int)cached_interpreter_table.BLEZL, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BLEZL_IDLE, 1);
    return;
     }
   
   genblez_test();
   gentest_idle();
   genblezl();
#endif
}

void genbgtzl(void)
{
#ifdef INTERPRET_BGTZL
   gencallinterp((unsigned int)cached_interpreter_table.BGTZL, 1);
#else
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
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
   if (((g_dev.r4300.recomp.dst->addr & 0xFFF) == 0xFFC && 
       (g_dev.r4300.recomp.dst->addr < 0x80000000 || g_dev.r4300.recomp.dst->addr >= 0xC0000000))||g_dev.r4300.recomp.no_compiled_jump)
     {
    gencallinterp((unsigned int)cached_interpreter_table.BGTZL_IDLE, 1);
    return;
     }
   
   genbgtz_test();
   gentest_idle();
   genbgtzl();
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

void genldl(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.LDL, 0);
}

void genldr(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.LDR, 0);
}

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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)&(*r4300_pc_struct()), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)&(*r4300_pc_struct()), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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

void genlwl(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.LWL, 0);
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)&(*r4300_pc_struct()), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)&(*r4300_pc_struct()), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)&(*r4300_pc_struct()), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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

void genlwr(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.LWR, 0);
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m8_reg8((unsigned char *)(memory_wbyte()), CL); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememb); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m16_reg16((unsigned short *)(memory_whword()), CX); // 7
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememh); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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

void genswl(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.SWL, 0);
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_reg32((unsigned int *)(memory_wword()), ECX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writemem); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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

void gensdl(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.SDL, 0);
}

void gensdr(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.SDR, 0);
}

void genswr(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.SWR, 0);
}

void gencheck_cop1_unusable(void)
{
   free_all_registers();
   simplify_access();
   test_m32_imm32((unsigned int*)&r4300_cp0_regs()[CP0_STATUS_REG], CP0_STATUS_CU1);
   jne_rj(0);

   jump_start_rel8();
   
   gencallinterp((unsigned int)dynarec_check_cop1_unusable, 0);
   
   jump_end_rel8();
}

void genlwc1(void)
{
#ifdef INTERPRET_LWC1
   gencallinterp((unsigned int)cached_interpreter_table.LWC1, 0);
#else
   gencheck_cop1_unusable();
   
   mov_eax_memoffs32((unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
   add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
   mov_reg32_reg32(EBX, EAX);
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
   mov_m32_reg32((unsigned int *)(&g_dev.mem.rdword), EDX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmem); // 7
   call_reg32(EBX); // 2
   jmp_imm_short(20); // 2
   
   and_reg32_imm32(EBX, 0x7FFFFF); // 6
   mov_reg32_preg32pimm32(EAX, EBX, (unsigned int)g_dev.ri.rdram.dram); // 6
   mov_reg32_m32(EBX, (unsigned int*)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
   mov_preg32_reg32(EBX, EAX); // 2
#endif
}

void genldc1(void)
{
#ifdef INTERPRET_LDC1
   gencallinterp((unsigned int)cached_interpreter_table.LDC1, 0);
#else
   gencheck_cop1_unusable();
   
   mov_eax_memoffs32((unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
   add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
   mov_reg32_reg32(EBX, EAX);
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
   mov_m32_reg32((unsigned int *)(&g_dev.mem.rdword), EDX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.readmemd); // 7
   call_reg32(EBX); // 2
   jmp_imm_short(32); // 2
   
   and_reg32_imm32(EBX, 0x7FFFFF); // 6
   mov_reg32_preg32pimm32(EAX, EBX, ((unsigned int)g_dev.ri.rdram.dram)+4); // 6
   mov_reg32_preg32pimm32(ECX, EBX, ((unsigned int)g_dev.ri.rdram.dram)); // 6
   mov_reg32_m32(EBX, (unsigned int*)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft])); // 6
   mov_preg32_reg32(EBX, EAX); // 2
   mov_preg32pimm32_reg32(EBX, 4, ECX); // 6
#endif
}

void gencache(void)
{
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_imm32((unsigned int *)(&g_dev.mem.rdword), (unsigned int)g_dev.r4300.recomp.dst->f.i.rt); // 10
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

void genswc1(void)
{
#ifdef INTERPRET_SWC1
   gencallinterp((unsigned int)cached_interpreter_table.SWC1, 0);
#else
   gencheck_cop1_unusable();
   
   mov_reg32_m32(EDX, (unsigned int*)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.lf.ft]));
   mov_reg32_preg32(ECX, EDX);
   mov_eax_memoffs32((unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
   add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
   mov_reg32_reg32(EBX, EAX);
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_reg32((unsigned int *)(memory_wword()), ECX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writemem); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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
   
   mov_reg32_m32(ESI, (unsigned int*)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.lf.ft]));
   mov_reg32_preg32(ECX, ESI);
   mov_reg32_preg32pimm32(EDX, ESI, 4);
   mov_eax_memoffs32((unsigned int *)(&r4300_regs()[g_dev.r4300.recomp.dst->f.lf.base]));
   add_eax_imm32((int)g_dev.r4300.recomp.dst->f.lf.offset);
   mov_reg32_reg32(EBX, EAX);
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_reg32((unsigned int *)(memory_wdword()), ECX); // 6
   mov_m32_reg32((unsigned int *)(memory_wdword())+1, EDX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememd); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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
   if(g_dev.r4300.recomp.fast_memory)
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
   
   mov_m32_imm32((unsigned int *)(&(*r4300_pc_struct())), (unsigned int)(g_dev.r4300.recomp.dst+1)); // 10
   mov_m32_reg32((unsigned int *)(memory_address()), EBX); // 6
   mov_m32_reg32((unsigned int *)(memory_wdword()), ECX); // 6
   mov_m32_reg32((unsigned int *)(memory_wdword())+1, EDX); // 6
   shr_reg32_imm8(EBX, 16); // 3
   mov_reg32_preg32x4pimm32(EBX, EBX, (unsigned int)g_dev.mem.writememd); // 7
   call_reg32(EBX); // 2
   mov_eax_memoffs32((unsigned int *)(memory_address())); // 5
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

void genll(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.LL, 0);
}

void gensc(void)
{
   gencallinterp((unsigned int)cached_interpreter_table.SC, 0);
}

