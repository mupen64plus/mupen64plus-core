;Mupen64plus - dyna_start.asm
;Mupen64Plus homepage: http://code.google.com/p/mupen64plus
;Copyright (C) 2007 Richard Goedeken (Richard42)
;Copyright (C) 2002 Hacktarux
;
;This program is free software; you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation; either version 2 of the License, or
;(at your option) any later version.
;
;This program is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.
;
;You should have received a copy of the GNU General Public License
;along with this program; if not, write to the
;Free Software Foundation, Inc.,
;51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

%include "../../src/main/asm_defines.h"

global dyna_start

extern g_dev

section .bss
align 4

section .rodata
section .text

dyna_start:
    push rbx                 ;we must push an even # of registers to keep stack 16-byte aligned
    push r12
    push r13
    push r14
    push r15
    push rbp
    mov [rel g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_rsp], rsp
    lea r15, [rel g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_regs]        ;store the base location of the r4300 registers in r15 for addressing
    call _A1
    jmp _A2
_A1:
    pop  rax
    mov  [rel g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_rip], rax
    sub rsp, 0x20
    and rsp, 0xFFFFFFFFFFFFFFF0          ;ensure that stack is 16-byte aligned
    mov rax, rsp
    sub rax, 8
    mov [rel g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_return_address], rax
    call rcx
_A2:
    mov  rsp, [rel g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_rsp]
    pop  rbp
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
