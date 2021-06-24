;Mupen64plus - linkage_x86.asm
;Copyright (C) 2009-2011 Ari64
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

%include "asm_defines_nasm.h"

%ifidn __OUTPUT_FORMAT__,elf
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf32
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

%ifdef LEADING_UNDERSCORE
    %macro  cglobal 1
      global  _%1
      %define %1 _%1
    %endmacro

    %macro  cextern 1
      extern  _%1
      %define %1 _%1
    %endmacro
%else
    %macro  cglobal 1
      global  %1
    %endmacro

    %macro  cextern 1
      extern  %1
    %endmacro
%endif

%macro get_GOT 0
      call  %%getgot
  %%getgot:
      pop  ebx
      add  ebx,_GLOBAL_OFFSET_TABLE_+$$-%%getgot wrt ..gotpc
%endmacro

%ifdef PIC
    %define get_got_address get_GOT
    %define find_local_data(a) ebx + a wrt ..gotoff
    %define find_external_data(a) ebx + a wrt ..got
%else
    %define get_got_address
    %define find_local_data(a) a
    %define find_extern_data(a) a
%endif

%define g_dev_r4300_new_dynarec_hot_state_stop              (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_new_dynarec_hot_state + offsetof_struct_new_dynarec_hot_state_stop)
%define g_dev_r4300_new_dynarec_hot_state_cycle_count       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_new_dynarec_hot_state + offsetof_struct_new_dynarec_hot_state_cycle_count)
%define g_dev_r4300_new_dynarec_hot_state_pending_exception (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_new_dynarec_hot_state + offsetof_struct_new_dynarec_hot_state_pending_exception)
%define g_dev_r4300_new_dynarec_hot_state_pcaddr            (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_new_dynarec_hot_state + offsetof_struct_new_dynarec_hot_state_pcaddr)

cglobal jump_vaddr_eax
cglobal jump_vaddr_ecx
cglobal jump_vaddr_edx
cglobal jump_vaddr_ebx
cglobal jump_vaddr_ebp
cglobal jump_vaddr_edi
cglobal verify_code
cglobal cc_interrupt
cglobal do_interrupt
cglobal fp_exception
cglobal jump_syscall
cglobal jump_eret
cglobal new_dyna_start
cglobal invalidate_block_eax
cglobal invalidate_block_ecx
cglobal invalidate_block_edx
cglobal invalidate_block_ebx
cglobal invalidate_block_ebp
cglobal invalidate_block_esi
cglobal invalidate_block_edi
cglobal breakpoint
cglobal dyna_linker
cglobal dyna_linker_ds

cextern base_addr
cextern new_recompile_block
cextern get_addr_ht
cextern get_addr
cextern dynarec_gen_interrupt
cextern clean_blocks
cextern invalidate_block
cextern ERET_new
cextern get_addr_32
cextern g_dev
cextern verify_dirty
cextern cop1_unusable
cextern SYSCALL_new
cextern dynamic_linker
cextern dynamic_linker_ds

%ifdef PIC
cextern _GLOBAL_OFFSET_TABLE_
%endif

section .bss
align 4

section .rodata
section .text

jump_vaddr_eax:
    mov     edi,    eax
    jmp     jump_vaddr_edi

jump_vaddr_ecx:
    mov     edi,    ecx
    jmp     jump_vaddr_edi

jump_vaddr_edx:
    mov     edi,    edx
    jmp     jump_vaddr_edi

jump_vaddr_ebx:
    mov     edi,    ebx
    jmp     jump_vaddr_edi

jump_vaddr_ebp:
    mov     edi,    ebp

jump_vaddr_edi:
    mov     eax,    edi

jump_vaddr:
    add     esp,    -12
    push    edi
    call    get_addr_ht
    add     esp,    16
    jmp     eax

verify_code:
    ;eax = head
    add     esp,    -8
    push    eax
    call    verify_dirty
    test    eax,eax
    jne     _D1
    add     esp,    12
    ret
_D1:
    add     esp,    4
    push    eax
    call    get_addr
    add     esp,    16
    jmp     eax

cc_interrupt:
    get_got_address
    mov     [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)],    esi    ;Count
    add     esp,    -28                 ;Align stack
    mov     DWORD [find_local_data(g_dev_r4300_new_dynarec_hot_state_pending_exception)],    0
    call    dynarec_gen_interrupt
    mov     esi,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)]
    mov     edx,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_pending_exception)]
    mov     ecx,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_stop)]
    add     esp,    28
    test    ecx,    ecx
    jne     _E2
    test    edx,    edx
    jne     _E1
    ret
_E1:
    add     esp,    -8
    mov     edi,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_pcaddr)]
    push    edi
    call    get_addr_ht
    add     esp,    16
    jmp     eax
_E2:
    add     esp,    4      ;pop return address

new_dyna_stop:
    add     esp,    12     ;pop stack
    pop     edi            ;restore edi
    pop     esi            ;restore esi
    pop     ebx            ;restore ebx
    pop     ebp            ;restore ebp
    ret                    ;exit dynarec

do_interrupt:
    get_got_address
    mov     ecx,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_stop)]
    test    ecx,    ecx
    jne     new_dyna_stop
    mov     edi,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_pcaddr)]
    add     esp,    -12
    push    edi
    call    get_addr_ht
    add     esp,    16
    mov     esi,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)]
    jmp     eax

fp_exception:
    get_got_address
    mov     [find_local_data(g_dev_r4300_new_dynarec_hot_state_pcaddr)],    eax
    call    cop1_unusable
    jmp     eax

jump_syscall:
    get_got_address
    mov     [find_local_data(g_dev_r4300_new_dynarec_hot_state_pcaddr)],    eax
    call    SYSCALL_new
    jmp     eax

jump_eret:
    get_got_address
    mov     [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)],    esi
    call    ERET_new
    mov     esi,    [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)]
    test    eax,   eax
    je      new_dyna_stop
    jmp     eax

dyna_linker:
    call    dynamic_linker
    add     esp,    8
    jmp     eax

dyna_linker_ds:
    call    dynamic_linker_ds
    add     esp,    8
    jmp     eax

new_dyna_start:
    push    ebp
    push    ebx
    push    esi
    push    edi
    add     esp,    -8    ;align stack
    push    0a4000040h
    call    new_recompile_block
    get_got_address
    mov     esi,    DWORD [find_local_data(g_dev_r4300_new_dynarec_hot_state_cycle_count)]
    jmp     DWORD [find_local_data(base_addr)]

invalidate_block_eax:
    push    eax
    push    ecx
    push    edx
    push    eax
    jmp     invalidate_block_call

invalidate_block_ecx:
    push    eax
    push    ecx
    push    edx
    push    ecx
    jmp     invalidate_block_call

invalidate_block_edx:
    push    eax
    push    ecx
    push    edx
    push    edx
    jmp     invalidate_block_call

invalidate_block_ebx:
    push    eax
    push    ecx
    push    edx
    push    ebx
    jmp     invalidate_block_call

invalidate_block_ebp:
    push    eax
    push    ecx
    push    edx
    push    ebp
    jmp     invalidate_block_call

invalidate_block_esi:
    push    eax
    push    ecx
    push    edx
    push    esi
    jmp     invalidate_block_call

invalidate_block_edi:
    push    eax
    push    ecx
    push    edx
    push    edi

invalidate_block_call:
    call    invalidate_block
    pop     eax ;Throw away
    pop     edx
    pop     ecx
    pop     eax
    ret

breakpoint:
    int    3
    ret
