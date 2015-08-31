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

global dyna_start

extern save_rsp
extern save_rip
extern reg
extern return_address

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
    mov [rel save_rsp], rsp
    lea r15, [rel reg]        ;store the base location of the r4300 registers in r15 for addressing
    call _A1
    jmp _A2
_A1:
    pop  rax
    mov  [rel save_rip], rax
    sub rsp, 0x20
    and rsp, 0xFFFFFFFFFFFFFFF0          ;ensure that stack is 16-byte aligned
    mov rax, rsp
    sub rax, 8
    mov [rel return_address], rax
    call rcx
_A2:
    mov  rsp, [rel save_rsp]
    pop  rbp
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
