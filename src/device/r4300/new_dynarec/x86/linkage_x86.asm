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

%include "main/asm_defines_nasm.h"

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

%define g_dev_ri_rdram_dram                    (g_dev + offsetof_struct_device_ri + offsetof_struct_ri_controller_rdram + offsetof_struct_rdram_dram)
%define g_dev_mem_address                      (g_dev + offsetof_struct_device_mem + offsetof_struct_memory_address)
%define g_dev_mem_wword                        (g_dev + offsetof_struct_device_mem + offsetof_struct_memory_wword)
%define g_dev_mem_wbyte                        (g_dev + offsetof_struct_device_mem + offsetof_struct_memory_wbyte)
%define g_dev_mem_whword                       (g_dev + offsetof_struct_device_mem + offsetof_struct_memory_whword)
%define g_dev_mem_wdword                       (g_dev + offsetof_struct_device_mem + offsetof_struct_memory_wdword)
%define g_dev_r4300_stop                       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_stop)
%define g_dev_r4300_regs                       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_regs)
%define g_dev_r4300_hi                         (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_hi)
%define g_dev_r4300_lo                         (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_lo)
%define g_dev_r4300_cp0_regs                   (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_cp0 + offsetof_struct_cp0_regs)
%define g_dev_r4300_cp0_next_interrupt         (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_cp0 + offsetof_struct_cp0_next_interrupt)
%define g_dev_r4300_cached_interp_invalid_code (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_cached_interp + offsetof_struct_cached_interp_invalid_code)

cglobal jump_vaddr_eax
cglobal jump_vaddr_ecx
cglobal jump_vaddr_edx
cglobal jump_vaddr_ebx
cglobal jump_vaddr_ebp
cglobal jump_vaddr_edi
cglobal verify_code_ds
cglobal verify_code_vm
cglobal verify_code
cglobal cc_interrupt
cglobal do_interrupt
cglobal fp_exception
cglobal fp_exception_ds
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
cglobal write_rdram_new
cglobal write_rdramb_new
cglobal write_rdramh_new
cglobal write_rdramd_new
cglobal read_nomem_new
cglobal read_nomemb_new
cglobal read_nomemh_new
cglobal read_nomemd_new
cglobal write_nomem_new
cglobal write_nomemb_new
cglobal write_nomemh_new
cglobal write_nomemd_new
cglobal write_mi_new
cglobal write_mib_new
cglobal write_mih_new
cglobal write_mid_new
cglobal breakpoint

cextern base_addr
cextern new_recompile_block
cextern get_addr_ht
cextern cycle_count
cextern get_addr
cextern branch_target
cextern memory_map
cextern pending_exception
cextern restore_candidate
cextern gen_interupt
cextern last_count
cextern pcaddr
cextern clean_blocks
cextern invalidate_block
cextern readmem_dword
cextern check_interupt
cextern get_addr_32
cextern write_mi
cextern write_mib
cextern write_mih
cextern write_mid
cextern TLB_refill_exception_new
cextern g_dev

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
    mov     [cycle_count],    esi    ;CCREG
    add     esi,    [last_count]
    mov     [g_dev_r4300_cp0_regs+36],    esi    ;Count
    call    get_addr_ht
    mov     esi,    [cycle_count]
    add     esp,    16
    jmp     eax

verify_code_ds:
    mov     [branch_target],    ebp

verify_code_vm:
    ;eax = source (virtual address)
    ;ebx = target
    ;ecx = length
    cmp     eax,    0C0000000h
    jl      verify_code
    mov     edx,    eax
    lea     ebp,    [-1+eax+ecx*1]
    shr     edx,    12
    shr     ebp,    12
    mov     edi,    [memory_map+edx*4]
    test    edi,    edi
    js      _D5
    lea     eax,    [eax+edi*4]
_D1:
    xor     edi,    [memory_map+edx*4]
    shl     edi,    2
    jne     _D5
    mov     edi,    [memory_map+edx*4]
    inc     edx
    cmp     edx,    ebp
    jbe     _D1

verify_code:
    ;eax = source
    ;ebx = target
    ;ecx = length
    mov     edi,    [-4+eax+ecx*1]
    xor     edi,    [-4+ebx+ecx*1]
    jne     _D5
    mov     edx,    ecx
    add     ecx,    -4
    je      _D3
    test    edx,    4
    cmove   ecx,    edx
    mov     [cycle_count],    esi
_D2:
    mov     edx,    [-4+eax+ecx*1]
    mov     ebp,    [-4+ebx+ecx*1]
    mov     esi,    [-8+eax+ecx*1]
    xor     ebp,    edx
    mov     edi,    [-8+ebx+ecx*1]
    jne     _D4
    xor     edi,    esi
    jne     _D4
    add     ecx,    -8
    jne     _D2
    mov     esi,    [cycle_count]
    mov     ebp,    [branch_target]
_D3:
    ret
_D4:
    mov     esi,    [cycle_count]
_D5:
    mov     ebp,    [branch_target]
    push    esi           ;for stack alignment, unused
    push    DWORD [8+esp]
    call    get_addr
    add     esp,    16    ;pop stack
    jmp     eax

cc_interrupt:
    add     esi,    [last_count]
    add     esp,    -28                 ;Align stack
    mov     [g_dev_r4300_cp0_regs+36],    esi    ;Count
    shr     esi,    19
    mov     DWORD [pending_exception],    0
    and     esi,    01fch
    cmp     DWORD [restore_candidate+esi],    0
    jne     _E4
_E1:
    call    gen_interupt
    mov     esi,    [g_dev_r4300_cp0_regs+36]
    mov     eax,    [g_dev_r4300_cp0_next_interrupt]
    mov     ebx,    [pending_exception]
    mov     ecx,    [g_dev_r4300_stop]
    add     esp,    28
    mov     [last_count],    eax
    sub     esi,    eax
    test    ecx,    ecx
    jne     _E3
    test    ebx,    ebx
    jne     _E2
    ret
_E2:
    add     esp,    -8
    mov     edi,    [pcaddr]
    mov     [cycle_count],    esi    ;CCREG
    push    edi
    call    get_addr_ht
    mov     esi,    [cycle_count]
    add     esp,    16
    jmp     eax
_E3:
    add     esp,    16     ;pop stack
    pop     edi            ;restore edi
    pop     esi            ;restore esi
    pop     ebx            ;restore ebx
    pop     ebp            ;restore ebp
    ret                    ;exit dynarec
_E4:
    ;Move 'dirty' blocks to the 'clean' list
    mov     ebx,    DWORD [restore_candidate+esi]
    mov     DWORD [restore_candidate+esi],    0
    shl     esi,    3
    mov     ebp,    0
_E5:
    shr     ebx,    1
    jnc     _E6
    mov     ecx,    esi
    add     ecx,    ebp
    push    ecx
    call    clean_blocks
    pop     ecx
_E6:
    inc     ebp
    test    ebp,    31
    jne     _E5
    jmp     _E1

do_interrupt:
    mov     edi,    [pcaddr]
    add     esp,    -12
    push    edi
    call    get_addr_ht
    add     esp,    16
    mov     esi,    [g_dev_r4300_cp0_regs+36]
    mov     ebx,    [g_dev_r4300_cp0_next_interrupt]
    mov     [last_count],    ebx
    sub     esi,    ebx
    add     esi,    2
    jmp     eax

fp_exception:
    mov     edx,    01000002ch
_E7:
    mov     ebx,    [g_dev_r4300_cp0_regs+48]
    add     esp,    -12
    or      ebx,    2
    mov     [g_dev_r4300_cp0_regs+48],    ebx     ;Status
    mov     [g_dev_r4300_cp0_regs+52],    edx     ;Cause
    mov     [g_dev_r4300_cp0_regs+56],    eax     ;EPC
    push    080000180h
    call    get_addr_ht
    add     esp,    16
    jmp     eax

fp_exception_ds:
    mov     edx,    09000002ch    ;Set high bit if delay slot
    jmp     _E7

jump_syscall:
    mov     edx,    020h
    mov     ebx,    [g_dev_r4300_cp0_regs+48]
    add     esp,    -12
    or      ebx,    2
    mov     [g_dev_r4300_cp0_regs+48],    ebx     ;Status
    mov     [g_dev_r4300_cp0_regs+52],    edx     ;Cause
    mov     [g_dev_r4300_cp0_regs+56],    eax     ;EPC
    push    080000180h
    call    get_addr_ht
    add     esp,    16
    jmp     eax

jump_eret:
    mov     ebx,    [g_dev_r4300_cp0_regs+48]        ;Status
    add     esi,    [last_count]
    and     ebx,    0FFFFFFFDh
    mov     [g_dev_r4300_cp0_regs+36],    esi        ;Count
    mov     [g_dev_r4300_cp0_regs+48],    ebx        ;Status
    call    check_interupt
    mov     eax,    [g_dev_r4300_cp0_next_interrupt]
    mov     esi,    [g_dev_r4300_cp0_regs+36]
    mov     [last_count],    eax
    sub     esi,    eax
    mov     eax,    [g_dev_r4300_cp0_regs+56]        ;EPC
    jns     _E11
_E8:
    mov     ebx,    248
    xor     edi,    edi
_E9:
    mov     ecx,    [g_dev_r4300_regs + ebx]
    mov     edx,    [g_dev_r4300_regs + ebx + 4]
    sar     ecx,    31
    xor     edx,    ecx
    neg     edx
    adc     edi,    edi
    sub     ebx,    8
    jne     _E9
    mov     ecx,    [g_dev_r4300_hi + ebx]
    mov     edx,    [g_dev_r4300_hi + ebx + 4]
    sar     ecx,    31
    xor     edx,    ecx
    jne     _E10
    mov     ecx,    [g_dev_r4300_lo + ebx]
    mov     edx,    [g_dev_r4300_lo + ebx + 4]
    sar     ecx,    31
    xor     edx,    ecx
_E10:
    neg     edx
    adc     edi,    edi
    add     esp,    -8
    push    edi
    push    eax
    mov     [cycle_count],    esi
    call    get_addr_32
    mov     esi,    [cycle_count]
    add     esp,    16
    jmp     eax
_E11:
    mov     [pcaddr],    eax
    call    cc_interrupt
    mov     eax,    [pcaddr]
    jmp     _E8

new_dyna_start:
    push    ebp
    push    ebx
    push    esi
    push    edi
    add     esp,    -8    ;align stack
    push    0a4000040h
    call    new_recompile_block
    mov     edi,    DWORD [g_dev_r4300_cp0_next_interrupt]
    mov     esi,    DWORD [g_dev_r4300_cp0_regs+36]
    mov     DWORD [last_count],    edi
    sub     esi,    edi
    jmp     DWORD [base_addr]

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

write_rdram_new:
    mov     edi,    [g_dev_mem_address]
    add     edi,    [g_dev_ri_rdram_dram]
    mov     ecx,    [g_dev_mem_wword]
    mov     [edi - 0x80000000],    ecx
    jmp     _E12

write_rdramb_new:
    mov     edi,    [g_dev_mem_address]
    xor     edi,    3
    add     edi,    [g_dev_ri_rdram_dram]
    mov     cl,     BYTE [g_dev_mem_wbyte]
    mov     BYTE [edi - 0x80000000],    cl
    jmp     _E12

write_rdramh_new:
    mov     edi,    [g_dev_mem_address]
    xor     edi,    2
    add     edi,    [g_dev_ri_rdram_dram]
    mov     cx,     WORD [g_dev_mem_whword]
    mov     WORD [edi - 0x80000000],    cx
    jmp     _E12

write_rdramd_new:
    mov     edi,    [g_dev_mem_address]
    add     edi,    [g_dev_ri_rdram_dram]
    mov     ecx,    [g_dev_mem_wdword+4]
    mov     edx,    [g_dev_mem_wdword+0]
    mov     [edi - 0x80000000],         ecx
    mov     [edi - 0x80000000 + 4],     edx
    jmp     _E12


do_invalidate:
    mov     edi,    [g_dev_mem_address]
    mov     ebx,    edi    ;Return ebx to caller
_E12:
    shr     edi,    12
    cmp     BYTE [g_dev_r4300_cached_interp_invalid_code + edi],    1
    je      _E13
    push    edi
    call    invalidate_block
    pop     edi
_E13:
    ret

read_nomem_new:
    mov     edi,    [g_dev_mem_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [memory_map+edi*4]
    mov     eax,    00h
    test    edi,    edi
    js      tlb_exception
    mov     ecx,    [ebx+edi*4]
    mov     [readmem_dword],    ecx
    ret

read_nomemb_new:
    mov     edi,    [g_dev_mem_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [memory_map+edi*4]
    mov     eax,    00h
    test    edi,    edi
    js      tlb_exception
    xor     ebx,    3
    movzx   ecx,    BYTE [ebx+edi*4]
    mov     [readmem_dword],    ecx
    ret

read_nomemh_new:
    mov     edi,    [g_dev_mem_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [memory_map+edi*4]
    mov     eax,    00h
    test    edi,    edi
    js      tlb_exception
    xor     ebx,    2
    movzx   ecx,    WORD [ebx+edi*4]
    mov     [readmem_dword],    ecx
    ret

read_nomemd_new:
    mov     edi,    [g_dev_mem_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [memory_map+edi*4]
    mov     eax,    00h
    test    edi,    edi
    js      tlb_exception
    mov     ecx,    [4+ebx+edi*4]
    mov     edx,    [ebx+edi*4]
    mov     [readmem_dword],      ecx
    mov     [readmem_dword+4],    edx
    ret

write_nomem_new:
    call    do_invalidate
    mov     edi,    [memory_map+edi*4]
    mov     ecx,    [g_dev_mem_wword]
    mov     eax,    01h
    shl     edi,    2
    jc      tlb_exception
    mov     [ebx+edi],    ecx
    ret

write_nomemb_new:
    call    do_invalidate
    mov     edi,    [memory_map+edi*4]
    mov     cl,     BYTE [g_dev_mem_wbyte]
    mov     eax,    01h
    shl     edi,    2
    jc      tlb_exception
    xor     ebx,    3
    mov     BYTE [ebx+edi],    cl
    ret

write_nomemh_new:
    call    do_invalidate
    mov     edi,    [memory_map+edi*4]
    mov     cx,     WORD [g_dev_mem_whword]
    mov     eax,    01h
    shl     edi,    2
    jc      tlb_exception
    xor     ebx,    2
    mov     WORD [ebx+edi],    cx
    ret

write_nomemd_new:
    call    do_invalidate
    mov     edi,    [memory_map+edi*4]
    mov     edx,    [g_dev_mem_wdword+4]
    mov     ecx,    [g_dev_mem_wdword+0]
    mov     eax,    01h
    shl     edi,    2
    jc      tlb_exception
    mov     [ebx+edi],    edx
    mov     [4+ebx+edi],    ecx
    ret

write_mi_new:
    mov     ebx,    [024h+esp]
    add     ebx,    4
    mov     [pcaddr],    ebx
    mov     DWORD [pending_exception],    0
    call    write_mi
    mov     ebx,    [pending_exception]
    test    ebx,    ebx
    jne     mi_exception
    ret

write_mib_new:
    mov     ebx,    [024h+esp]
    add     ebx,    4
    mov     [pcaddr],    ebx
    mov     DWORD [pending_exception],    0
    call    write_mib
    mov     ebx,    [pending_exception]
    test    ebx,    ebx
    jne     mi_exception
    ret

write_mih_new:
    mov     ebx,    [024h+esp]
    add     ebx,    4
    mov     [pcaddr],    ebx
    mov     DWORD [pending_exception],    0
    call    write_mih
    mov     ebx,    [pending_exception]
    test    ebx,    ebx
    jne     mi_exception
    ret

write_mid_new:
    mov     ebx,    [024h+esp]
    add     ebx,    4
    mov     [pcaddr],    ebx
    mov     DWORD [pending_exception],    0
    call    write_mid
    mov     ebx,    [pending_exception]
    test    ebx,    ebx
    jne     mi_exception
    ret

mi_exception:
    ;ebx = mem addr
    ;ebp = instr addr + flags
    mov     ebp,    [024h+esp]
    mov     ebx,    [g_dev_mem_address]
    add     esp,    024h
    call    wb_base_reg
    jmp     do_interrupt

tlb_exception:
    ;eax = r/w
    ;ebx = mem addr
    ;ebp = instr addr + flags
    mov     ebp,    [024h+esp]
    add     esp,    024h
    call    wb_base_reg
    add     esp,    -4
    push    eax
    push    ebx
    push    ebp
    call    TLB_refill_exception_new
    add     esp,    16
    mov     edi,    DWORD [g_dev_r4300_cp0_next_interrupt]
    mov     esi,    DWORD [g_dev_r4300_cp0_regs+36]    ;Count
    mov     DWORD [last_count],    edi
    sub     esi,    edi
    jmp     eax

wb_base_reg:
    ;ebx = address
    ;ebp = instr addr + flags
    mov     ecx,    ebp
    mov     edx,    ebp
    shr     ecx,    12
    and     edx,    0FFFFFFFCh
    mov     ecx,    [memory_map+ecx*4]
    mov     ecx,    [edx+ecx*4]
    mov     edx,    06000022h
    mov     edi,    ecx
    movsx   esi,    cx
    shr     ecx,    26
    shr     edi,    21
    sub     ebx,    esi
    add     esi,    ebx
    and     edi,    01fh
    rcr     edx,    cl
    cmovc   ebx,    [g_dev_r4300_regs+edi*8]
    mov     [g_dev_r4300_regs+edi*8],    ebx
    sar     ebx,    31
    test    ebp,    2
    cmove   ebx,    [g_dev_r4300_regs+4+edi*8]
    mov     [g_dev_r4300_regs+4+edi*8],    ebx
    mov     ebx,    esi
    ret

breakpoint:
    int    3
    ret
