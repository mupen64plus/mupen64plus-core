/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - linkage_arm.S                                           *
 *   Copyright (C) 2009-2011 Ari64                                         *
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

#define GLOBAL_FUNCTION(name)  \
    .align 2;                  \
    .globl name;               \
    .hidden name;              \
    .type name, %function;     \
    name

#define LOCAL_FUNCTION(name)  \
    .align 2;                 \
    .hidden name;             \
    .type name, %function;    \
    name

#define GLOBAL_VARIABLE(name, size_) \
    .global name;                    \
    .hidden name;                    \
    .type   name, %object;           \
    .size   name, size_

#define TEXT_SECTION .text
#define END_SECTION

#ifndef __ARM_NEON__
#if (defined(__VFP_FP__) && !defined(__SOFTFP__) && defined(__ARM_PCS_VFP))
    .fpu vfp
#else
    .fpu softvfp
#endif
#else
    .fpu neon
#endif
    .eabi_attribute 20, 1
    .eabi_attribute 21, 1
#ifndef __ARM_NEON__
    .eabi_attribute 23, 3
#endif
    .eabi_attribute 24, 1
    .eabi_attribute 25, 1
    .eabi_attribute 26, 2
#ifndef __ARM_NEON__
#if (defined(__VFP_FP__) && !defined(__SOFTFP__) && defined(__ARM_PCS_VFP))
    .eabi_attribute 28, 1
#endif
#endif
    .eabi_attribute 30, 6
    .eabi_attribute 18, 4
    .file    "linkage_arm.S"

#include "asm_defines_gas.h"

device_r4300_new_dynarec_hot_state_dynarec_local = (offsetof_struct_device_r4300 + offsetof_struct_r4300_core_new_dynarec_hot_state + offsetof_struct_new_dynarec_hot_state_dynarec_local)

/* Defines offsets for fp addressed variables */
fp_saved_context       = offsetof_struct_new_dynarec_hot_state_dynarec_local + 28
fp_cycle_count         = offsetof_struct_new_dynarec_hot_state_cycle_count
fp_pending_exception   = offsetof_struct_new_dynarec_hot_state_pending_exception
fp_pcaddr              = offsetof_struct_new_dynarec_hot_state_pcaddr
fp_stop                = offsetof_struct_new_dynarec_hot_state_stop

TEXT_SECTION

    .align   2
    .outptr_offset  : .word out-(.outptr_pic+8)
    .savedcontextptr_offset  : .word g_dev + device_r4300_new_dynarec_hot_state_dynarec_local + fp_saved_context -(.savedcontextptr_pic+8)

GLOBAL_FUNCTION(jump_vaddr_r0):
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r1):
    mov    r0, r1
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r2):
    mov    r0, r2
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r3):
    mov    r0, r3
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r4):
    mov    r0, r4
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r5):
    mov    r0, r5
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r6):
    mov    r0, r6
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r8):
    mov    r0, r8
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r9):
    mov    r0, r9
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r10):
    mov    r0, r10
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r12):
    mov    r0, r12
    b      jump_vaddr

GLOBAL_FUNCTION(jump_vaddr_r7):
    add    r0, r7, #0

GLOBAL_FUNCTION(jump_vaddr):
    bl     get_addr_ht
    mov    pc, r0

GLOBAL_FUNCTION(verify_code):
    /* r0 = head */
    mov    r9, lr
    bl     verify_dirty
    tst    r0, r0
    moveq  pc, r9
    bl     get_addr
    mov    pc, r0

GLOBAL_FUNCTION(cc_interrupt):
    str    r10, [fp, #fp_cycle_count]
    mov    r1, #0
    str    r1, [fp, #fp_pending_exception]
    mov    r10, lr
    bl     dynarec_gen_interrupt
    mov    lr, r10
    ldr    r10, [fp, #fp_cycle_count]
    ldr    r1, [fp, #fp_pending_exception]
    ldr    r2, [fp, #fp_stop]
    tst    r2, r2
    bne    new_dyna_stop
    tst    r1, r1
    moveq  pc, lr
.E1:
    ldr    r0, [fp, #fp_pcaddr]
    bl     get_addr_ht
    mov    pc, r0

LOCAL_FUNCTION(new_dyna_stop):
    add    r12, fp, #fp_saved_context
    ldmia  r12, {r4, r5, r6, r7, r8, r9, sl, fp, pc}

GLOBAL_FUNCTION(do_interrupt):
    ldr    r2, [fp, #fp_stop]
    tst    r2, r2
    bne    new_dyna_stop
    ldr    r0, [fp, #fp_pcaddr]
    bl     get_addr_ht
    ldr    r10, [fp, #fp_cycle_count]
    mov    pc, r0

GLOBAL_FUNCTION(fp_exception):
    str    r0, [fp, #fp_pcaddr]
    bl     cop1_unusable
    mov    pc, r0

GLOBAL_FUNCTION(jump_syscall):
    str    r0, [fp, #fp_pcaddr]
    bl     SYSCALL_new
    mov    pc, r0

GLOBAL_FUNCTION(jump_eret):
    str    r10, [fp, #fp_cycle_count]
    bl     ERET_new
    ldr    r10, [fp, #fp_cycle_count]
    tst    r0, r0
    beq    new_dyna_stop
    mov    pc, r0

GLOBAL_FUNCTION(dyna_linker):
    bl     dynamic_linker
    mov    pc, r0

GLOBAL_FUNCTION(dyna_linker_ds):
    bl     dynamic_linker_ds
    mov    pc, r0

GLOBAL_FUNCTION(new_dyna_start):
    ldr    r12, .savedcontextptr_offset
.savedcontextptr_pic:
    add    r12, pc, r12
    ldr    r1, .outptr_offset
.outptr_pic:
    add    r1, pc, r1
    mov    r0, #0xa4000000
    stmia  r12, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    sub    fp, r12, #fp_saved_context
    ldr    r4, [r1]
    add    r0, r0, #0x40
    bl     new_recompile_block
    ldr    r10, [fp, #fp_cycle_count]
    mov    pc, r4

GLOBAL_FUNCTION(invalidate_addr_r0):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r0, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r1):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r1, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r2):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r2, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r3):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r3, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r4):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r4, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r5):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r5, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r6):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r6, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r7):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r7, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r8):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r8, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r9):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r9, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r10):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r10, #12
    b      invalidate_addr_call

GLOBAL_FUNCTION(invalidate_addr_r12):
    stmia  fp, {r0, r1, r2, r3, r12, lr}
    lsr    r0, r12, #12

LOCAL_FUNCTION(invalidate_addr_call):
    bl     invalidate_block
    ldmia  fp, {r0, r1, r2, r3, r12, pc}

GLOBAL_FUNCTION(breakpoint):
    .inst 0xe7f001f0
    mov    pc, lr

END_SECTION
