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
cextern gen_interrupt
cextern last_count
cextern pcaddr
cextern clean_blocks
cextern invalidate_block
cextern readmem_dword
cextern new_dynarec_check_interrupt
cextern get_addr_32
cextern write_mi
cextern write_mib
cextern write_mih
cextern write_mid
cextern TLB_refill_exception_new
cextern g_dev

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
    get_got_address
    add     esp,    -12
    push    edi
    mov     [find_local_data(cycle_count)],    esi    ;CCREG
    add     esi,    [find_local_data(last_count)]
    mov     [find_local_data(g_dev_r4300_cp0_regs+36)],    esi    ;Count
    call    get_addr_ht
    mov     esi,    [find_local_data(cycle_count)]
    add     esp,    16
    jmp     eax

verify_code_ds:
    ;eax = source (virtual address)
    ;edx = target
    ;ecx = length
    get_got_address
    mov     [find_local_data(branch_target)],    ebp

verify_code_vm:
    ;eax = source (virtual address)
    ;edx = target
    ;ecx = length
    get_got_address
    cmp     eax,    0C0000000h
    jl      verify_code
    mov     [find_local_data(cycle_count)],    esi
    mov     esi,    eax
    lea     ebp,    [-1+eax+ecx*1]
    shr     esi,    12
    shr     ebp,    12
    mov     edi,    [find_local_data(memory_map+esi*4)]
    test    edi,    edi
    js      _D4
    lea     eax,    [eax+edi*4]
_D1:
    xor     edi,    [find_local_data(memory_map+esi*4)]
    shl     edi,    2
    jne     _D4
    mov     edi,    [find_local_data(memory_map+esi*4)]
    inc     esi
    cmp     esi,    ebp
    jbe     _D1
    mov     esi,    [find_local_data(cycle_count)]

verify_code:
    ;eax = source
    ;edx = target
    ;ecx = length
    get_got_address
    mov     edi,    [-4+eax+ecx*1]
    xor     edi,    [-4+edx+ecx*1]
    jne     _D5
    mov     edi,    ecx
    add     ecx,    -4
    je      _D3
    test    edi,    4
    cmove   ecx,    edi
_D2:
    mov     edi,    [-4+eax+ecx*1]
    xor     edi,    [-4+edx+ecx*1]
    jne     _D5
    mov     edi,    [-8+eax+ecx*1]
    xor     edi,    [-8+edx+ecx*1]
    jne     _D5
    add     ecx,    -8
    jne     _D2
_D3:
    mov     ebp,    [find_local_data(branch_target)]
    ret
_D4:
    mov     esi,    [find_local_data(cycle_count)]
_D5:
    mov     ebp,    [find_local_data(branch_target)]
    push    esi           ;for stack alignment, unused
    push    DWORD [8+esp]
    call    get_addr
    add     esp,    16    ;pop stack
    jmp     eax

cc_interrupt:
    get_got_address
    add     esi,    [find_local_data(last_count)]
    add     esp,    -28                 ;Align stack
    mov     [find_local_data(g_dev_r4300_cp0_regs+36)],    esi    ;Count
    shr     esi,    19
    mov     DWORD [find_local_data(pending_exception)],    0
    and     esi,    01fch
    cmp     DWORD [find_local_data(restore_candidate+esi)],    0
    jne     _E4
_E1:
    call    gen_interrupt
    mov     esi,    [find_local_data(g_dev_r4300_cp0_regs+36)]
    mov     eax,    [find_local_data(g_dev_r4300_cp0_next_interrupt)]
    mov     edx,    [find_local_data(pending_exception)]
    mov     ecx,    [find_local_data(g_dev_r4300_stop)]
    add     esp,    28
    mov     [find_local_data(last_count)],    eax
    sub     esi,    eax
    test    ecx,    ecx
    jne     _E3
    test    edx,    edx
    jne     _E2
    ret
_E2:
    add     esp,    -8
    mov     edi,    [find_local_data(pcaddr)]
    mov     [find_local_data(cycle_count)],    esi    ;CCREG
    push    edi
    call    get_addr_ht
    mov     esi,    [find_local_data(cycle_count)]
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
    mov     edi,    DWORD [find_local_data(restore_candidate+esi)]
    mov     DWORD [find_local_data(restore_candidate+esi)],    0
    shl     esi,    3
    mov     ebp,    0
_E5:
    shr     edi,    1
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
    get_got_address
    mov     edi,    [find_local_data(pcaddr)]
    add     esp,    -12
    push    edi
    call    get_addr_ht
    add     esp,    16
    mov     esi,    [find_local_data(g_dev_r4300_cp0_regs+36)]
    mov     edx,    [find_local_data(g_dev_r4300_cp0_next_interrupt)]
    mov     [find_local_data(last_count)],    edx
    sub     esi,    edx
    add     esi,    2
    jmp     eax

fp_exception:
    mov     edx,    01000002ch
_E7:
    get_got_address
    mov     ecx,    [find_local_data(g_dev_r4300_cp0_regs+48)]
    add     esp,    -12
    or      ecx,    2
    mov     [find_local_data(g_dev_r4300_cp0_regs+48)],    ecx     ;Status
    mov     [find_local_data(g_dev_r4300_cp0_regs+52)],    edx     ;Cause
    mov     [find_local_data(g_dev_r4300_cp0_regs+56)],    eax     ;EPC
    push    080000180h
    call    get_addr_ht
    add     esp,    16
    jmp     eax

fp_exception_ds:
    mov     edx,    09000002ch    ;Set high bit if delay slot
    jmp     _E7

jump_syscall:
    get_got_address
    mov     edx,    020h
    mov     ecx,    [find_local_data(g_dev_r4300_cp0_regs+48)]
    add     esp,    -12
    or      ecx,    2
    mov     [find_local_data(g_dev_r4300_cp0_regs+48)],    ecx     ;Status
    mov     [find_local_data(g_dev_r4300_cp0_regs+52)],    edx     ;Cause
    mov     [find_local_data(g_dev_r4300_cp0_regs+56)],    eax     ;EPC
    push    080000180h
    call    get_addr_ht
    add     esp,    16
    jmp     eax

jump_eret:
    get_got_address
    mov     ecx,    [find_local_data(g_dev_r4300_cp0_regs+48)]        ;Status
    add     esi,    [find_local_data(last_count)]
    and     ecx,    0FFFFFFFDh
    mov     [find_local_data(g_dev_r4300_cp0_regs+36)],    esi        ;Count
    mov     [find_local_data(g_dev_r4300_cp0_regs+48)],    ecx        ;Status
    call    new_dynarec_check_interrupt
    mov     eax,    [find_local_data(g_dev_r4300_cp0_next_interrupt)]
    mov     esi,    [find_local_data(g_dev_r4300_cp0_regs+36)]
    mov     [find_local_data(last_count)],    eax
    sub     esi,    eax
    mov     [find_local_data(cycle_count)],    esi
    mov     eax,    [find_local_data(g_dev_r4300_cp0_regs+56)]        ;EPC
    jns     _E11
_E8:
    mov     esi,    248
    xor     edi,    edi
_E9:
    mov     ecx,    [find_local_data(g_dev_r4300_regs + esi)]
    mov     edx,    [find_local_data(g_dev_r4300_regs + esi + 4)]
    sar     ecx,    31
    xor     edx,    ecx
    neg     edx
    adc     edi,    edi
    sub     esi,    8
    jne     _E9
    mov     ecx,    [find_local_data(g_dev_r4300_hi + esi)]
    mov     edx,    [find_local_data(g_dev_r4300_hi + esi + 4)]
    sar     ecx,    31
    xor     edx,    ecx
    jne     _E10
    mov     ecx,    [find_local_data(g_dev_r4300_lo + esi)]
    mov     edx,    [find_local_data(g_dev_r4300_lo + esi + 4)]
    sar     ecx,    31
    xor     edx,    ecx
_E10:
    neg     edx
    adc     edi,    edi
    add     esp,    -8
    push    edi
    push    eax
    call    get_addr_32
    mov     esi,    [find_local_data(cycle_count)]
    add     esp,    16
    jmp     eax
_E11:
    mov     [find_local_data(pcaddr)],    eax
    call    cc_interrupt
    mov     eax,    [find_local_data(pcaddr)]
    jmp     _E8

new_dyna_start:
    push    ebp
    push    ebx
    push    esi
    push    edi
    add     esp,    -8    ;align stack
    push    0a4000040h
    call    new_recompile_block
    get_got_address
    mov     edi,    DWORD [find_local_data(g_dev_r4300_cp0_next_interrupt)]
    mov     esi,    DWORD [find_local_data(g_dev_r4300_cp0_regs+36)]
    mov     DWORD [find_local_data(last_count)],    edi
    sub     esi,    edi
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

write_rdram_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    add     edx,    [find_local_data(g_dev_ri_rdram_dram)]
    mov     ecx,    [find_local_data(g_dev_mem_wword)]
    mov     [edx - 0x80000000],    ecx
    jmp     _E12

write_rdramb_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    xor     edx,    3
    add     edx,    [find_local_data(g_dev_ri_rdram_dram)]
    mov     cl,     BYTE [find_local_data(g_dev_mem_wbyte)]
    mov     BYTE [edx - 0x80000000],    cl
    jmp     _E12

write_rdramh_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    xor     edx,    2
    add     edx,    [find_local_data(g_dev_ri_rdram_dram)]
    mov     cx,     WORD [find_local_data(g_dev_mem_whword)]
    mov     WORD [edx - 0x80000000],    cx
    jmp     _E12

write_rdramd_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    add     edx,    [find_local_data(g_dev_ri_rdram_dram)]
    mov     ecx,    [find_local_data(g_dev_mem_wdword+4)]
    mov     [edx - 0x80000000],         ecx
    mov     ecx,    [find_local_data(g_dev_mem_wdword+0)]
    mov     [edx - 0x80000000 + 4],     ecx
    jmp     _E12

do_invalidate:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    mov     edi,    edx    ;Return edi to caller
_E12:
    shr     edx,    12
    cmp     BYTE [find_local_data(g_dev_r4300_cached_interp_invalid_code + edx)],    1
    je      _E13
    push    edx
    call    invalidate_block
    pop     edx
_E13:
    ret

read_nomem_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    mov     edi,    edx
    shr     edx,    12
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    00h
    test    edx,    edx
    js      tlb_exception
    mov     ecx,    [edi+edx*4]
    mov     [find_local_data(readmem_dword)],    ecx
    ret

read_nomemb_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    mov     edi,    edx
    shr     edx,    12
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    00h
    test    edx,    edx
    js      tlb_exception
    xor     edi,    3
    movzx   ecx,    BYTE [edi+edx*4]
    mov     [find_local_data(readmem_dword)],    ecx
    ret

read_nomemh_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    mov     edi,    edx
    shr     edx,    12
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    00h
    test    edx,    edx
    js      tlb_exception
    xor     edi,    2
    movzx   ecx,    WORD [edi+edx*4]
    mov     [find_local_data(readmem_dword)],    ecx
    ret

read_nomemd_new:
    get_got_address
    mov     edx,    [find_local_data(g_dev_mem_address)]
    mov     edi,    edx
    shr     edx,    12
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    00h
    test    edx,    edx
    js      tlb_exception
    mov     ecx,    [4+edi+edx*4]
    mov     [find_local_data(readmem_dword)],      ecx
    mov     ecx,    [edi+edx*4]
    mov     [find_local_data(readmem_dword+4)],    ecx
    ret

write_nomem_new:
    call    do_invalidate
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    01h
    shl     edx,    2
    jc      tlb_exception
    mov     ecx,    [find_local_data(g_dev_mem_wword)]
    mov     [edi+edx],    ecx
    ret

write_nomemb_new:
    call    do_invalidate
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    01h
    shl     edx,    2
    jc      tlb_exception
    xor     edi,    3
    mov     cl,     BYTE [find_local_data(g_dev_mem_wbyte)]
    mov     BYTE [edi+edx],    cl
    ret

write_nomemh_new:
    call    do_invalidate
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    01h
    shl     edx,    2
    jc      tlb_exception
    xor     edi,    2
    mov     cx,     WORD [find_local_data(g_dev_mem_whword)]
    mov     WORD [edi+edx],    cx
    ret

write_nomemd_new:
    call    do_invalidate
    mov     edx,    [find_local_data(memory_map+edx*4)]
    mov     eax,    01h
    shl     edx,    2
    jc      tlb_exception
    mov     ecx,    [find_local_data(g_dev_mem_wdword+4)]
    mov     [edi+edx],    ecx
    mov     ecx,    [find_local_data(g_dev_mem_wdword+0)]
    mov     [4+edi+edx],    ecx
    ret

write_mi_new:
    get_got_address
    mov     ecx,    [024h+esp]
    add     ecx,    4
    mov     [find_local_data(pcaddr)],    ecx
    mov     DWORD [find_local_data(pending_exception)],    0
    call    write_mi
    mov     ecx,    [find_local_data(pending_exception)]
    test    ecx,    ecx
    jne     mi_exception
    ret

write_mib_new:
    get_got_address
    mov     ecx,    [024h+esp]
    add     ecx,    4
    mov     [find_local_data(pcaddr)],    ecx
    mov     DWORD [find_local_data(pending_exception)],    0
    call    write_mib
    mov     ecx,    [find_local_data(pending_exception)]
    test    ecx,    ecx
    jne     mi_exception
    ret

write_mih_new:
    get_got_address
    mov     ecx,    [024h+esp]
    add     ecx,    4
    mov     [find_local_data(pcaddr)],    ecx
    mov     DWORD [find_local_data(pending_exception)],    0
    call    write_mih
    mov     ecx,    [find_local_data(pending_exception)]
    test    ecx,    ecx
    jne     mi_exception
    ret

write_mid_new:
    get_got_address
    mov     ecx,    [024h+esp]
    add     ecx,    4
    mov     [find_local_data(pcaddr)],    ecx
    mov     DWORD [find_local_data(pending_exception)],    0
    call    write_mid
    mov     ecx,    [find_local_data(pending_exception)]
    test    ecx,    ecx
    jne     mi_exception
    ret

mi_exception:
;Input:
    ;esp+0x24 = instr addr + flags
;Output:
    ;None
    mov     edi,    [find_local_data(g_dev_mem_address)]
    add     esp,    024h
    call    wb_base_reg
    jmp     do_interrupt

tlb_exception:
;Input:
    ;eax = r/w
    ;edi = mem addr
    ;esp+0x24 = instr addr + flags
;Output:
    ;None
    add     esp,    024h
    call    wb_base_reg
    add     esp,    -4
    push    eax
    push    edi
    push    ecx
    call    TLB_refill_exception_new
    add     esp,    16
    mov     edi,    DWORD [find_local_data(g_dev_r4300_cp0_next_interrupt)]
    mov     esi,    DWORD [find_local_data(g_dev_r4300_cp0_regs+36)]    ;Count
    mov     DWORD [find_local_data(last_count)],    edi
    sub     esi,    edi
    jmp     eax

wb_base_reg:
;Input:
    ;edi = address
    ;esp+4 = instr addr + flags
;Output:
    ;ecx = instr addr + flags
    mov     ecx,    [04h+esp]
    mov     edx,    [04h+esp]
    shr     ecx,    12
    and     edx,    0FFFFFFFCh
    mov     ecx,    [find_local_data(memory_map+ecx*4)]
    mov     ecx,    [edx+ecx*4]
    mov     edx,    06000022h
    mov     ebp,    ecx
    movsx   esi,    cx
    shr     ecx,    26
    shr     ebp,    21
    sub     edi,    esi
    add     esi,    edi
    and     ebp,    01fh
    rcr     edx,    cl
    cmovc   edi,    [find_local_data(g_dev_r4300_regs+ebp*8)]
    mov     [find_local_data(g_dev_r4300_regs+ebp*8)],    edi
    sar     edi,    31
    mov     ecx,    [04h+esp]
    test    ecx,    2
    cmove   edi,    [find_local_data(g_dev_r4300_regs+4+ebp*8)]
    mov     [find_local_data(g_dev_r4300_regs+4+ebp*8)],    edi
    mov     edi,    esi
    ret

breakpoint:
    int    3
    ret
