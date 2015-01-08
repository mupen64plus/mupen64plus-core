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

section .text

extern _base_addr
extern _tlb_LUT_r
extern _jump_in
extern _add_link
extern _hash_table
extern _jump_dirty
extern _new_recompile_block
extern _g_cp0_regs
extern _get_addr_ht
extern _cycle_count
extern _get_addr
extern _branch_target
extern _memory_map
extern _pending_exception
extern _restore_candidate
extern _gen_interupt
extern _next_interupt
extern _stop
extern _last_count
extern _pcaddr
extern _clean_blocks
extern _reg
extern _hi
extern _lo
extern _invalidate_block
extern _address
extern _rdram
extern _cpu_byte
extern _hword
extern _word
extern _invalid_code
extern _readmem_dword
extern _check_interupt
extern _get_addr_32

global _dyna_linker

_dyna_linker:
    ;eax = virtual target _address
    ;ebx = instruction to patch
    mov     edi,    eax
    mov     ecx,    eax
    shr     edi,    12
    cmp     eax,    0C0000000h
    cmovge  ecx,    [_tlb_LUT_r+edi*4]
    test    ecx,    ecx
    cmovz   ecx,    eax
    xor     ecx,    080000000h
    mov     edx,    2047
    shr     ecx,    12
    and     edx,    ecx
    or      edx,    2048
    cmp     ecx,    edx
    cmova   ecx,    edx
    ;_jump_in lookup
    mov     edx,    [_jump_in+ecx*4]
_A1:
    test    edx,    edx
    je      _A3
    mov     edi,    [edx]
    xor     edi,    eax
    or      edi,    [4+edx]
    je      _A2
    mov     edx,    DWORD [12+edx]
    jmp     _A1
_A2:
    mov     edi,    [ebx]
    mov     ebp,    esi
    lea     esi,    [4+ebx+edi*1]
    mov     edi,    eax
    pusha
    call     _add_link
    popa
    mov     edi,    [8+edx]
    mov     esi,    ebp
    lea     edx,    [-4+edi]
    sub     edx,    ebx
    mov     DWORD [ebx],    edx
    jmp     edi
_A3:
    ;_hash_table lookup
    mov     edi,    eax
    mov     edx,    eax
    shr     edi,    16
    shr     edx,    12
    xor     edi,    eax
    and     edx,    2047
    movzx   edi,    di
    shl     edi,    4
    cmp     ecx,    2048
    cmovc   ecx,    edx
    cmp     eax,    [_hash_table+edi]
    jne     _A5
_A4:
    mov     edx,    [_hash_table+4+edi]
    jmp     edx
_A5:
    cmp     eax,    [_hash_table+8+edi]
    lea     edi,    [8+edi]
    je         _A4
    ;_jump_dirty lookup
    mov     edx,    [_jump_dirty+ecx*4]
_A6:
    test    edx,    edx
    je         _A8
    mov     ecx,    [edx]
    xor     ecx,    eax
    or      ecx,    [4+edx]
    je      _A7
    mov     edx,    DWORD [12+edx]
    jmp     _A6
_A7:
    mov     edx,    [8+edx]
    ;_hash_table insert
    mov     ebx,    [_hash_table-8+edi]
    mov     ecx,    [_hash_table-4+edi]
    mov     [_hash_table-8+edi],    eax
    mov     [_hash_table-4+edi],    edx
    mov     [_hash_table+edi],      ebx
    mov     [_hash_table+4+edi],    ecx
    jmp     edx
_A8:
    mov     edi,    eax
    pusha
    call    _new_recompile_block
    test    eax,    eax
    popa
    je      _dyna_linker
    ;pagefault
    mov     ebx,    eax
    mov     ecx,    008h

exec_pagefault:
    ;eax = instruction pointer
    ;ebx = fault _address
    ;ecx = cause
    mov     edx,    [_g_cp0_regs+48]
    add     esp,    -12
    mov     edi,    [_g_cp0_regs+16]
    or      edx,    2
    mov     [_g_cp0_regs+32],    ebx        ;BadVAddr
    and     edi,    0FF80000Fh
    mov     [_g_cp0_regs+48],    edx        ;Status
    mov     [_g_cp0_regs+52],    ecx        ;Cause
    mov     [_g_cp0_regs+56],    eax        ;EPC
    mov     ecx,    ebx
    shr     ebx,    9
    and     ecx,    0FFFFE000h
    and     ebx,    0007FFFF0h
    mov     [_g_cp0_regs+40],    ecx        ;EntryHI
    or      edi,    ebx
    mov     [_g_cp0_regs+16],    edi        ;Context
    push     080000000h
    call    _get_addr_ht
    add     esp,    16
    jmp     eax

global _dyna_linker_ds
    ;Special dynamic linker for the case where a page fault
    ;may occur in a branch delay slot
_dyna_linker_ds:
    mov     edi,    eax
    mov     ecx,    eax
    shr     edi,    12
    cmp     eax,    0C0000000h
    cmovge  ecx,    [_tlb_LUT_r+edi*4]
    test    ecx,    ecx
    cmovz   ecx,    eax
    xor     ecx,    080000000h
    mov     edx,    2047
    shr     ecx,    12
    and     edx,    ecx
    or      edx,    2048
    cmp     ecx,    edx
    cmova   ecx,    edx
    ;_jump_in lookup
    mov     edx,    [_jump_in+ecx*4]
_B1:
    test    edx,    edx
    je         _B3
    mov     edi,    [edx]
    xor     edi,    eax
    or      edi,    [4+edx]
    je      _B2
    mov     edx,    DWORD [12+edx]
    jmp     _B1
_B2:
    mov     edi,    [ebx]
    mov     ecx,    esi
    lea     esi,    [4+ebx+edi*1]
    mov     edi,    eax
    pusha
    call    _add_link
    popa
    mov     edi,    [8+edx]
    mov     esi,    ecx
    lea     edx,    [-4+edi]
    sub     edx,    ebx
    mov     DWORD [ebx],    edx
    jmp     edi
_B3:
    ;_hash_table lookup
    mov     edi,    eax
    mov     edx,    eax
    shr     edi,    16
    shr     edx,    12
    xor     edi,    eax
    and     edx,    2047
    movzx   edi,    di
    shl     edi,    4
    cmp     ecx,    2048
    cmovc   ecx,    edx
    cmp     eax,    [_hash_table+edi]
    jne     _B5
_B4:
    mov     edx,    [_hash_table+4+edi]
    jmp     edx
_B5:
    cmp     eax,    [_hash_table+8+edi]
    lea     edi,    [8+edi]
    je      _B4
    ;_jump_dirty lookup
    mov     edx,    [_jump_dirty+ecx*4]
_B6:
    test    edx,    edx
    je      _B8
    mov     ecx,    [edx]
    xor     ecx,    eax
    or      ecx,    [4+edx]
    je      _B7
    mov     edx,    DWORD [12+edx]
    jmp     _B6
_B7:
    mov     edx,    [8+edx]
    ;_hash_table insert
    mov     ebx,    [_hash_table-8+edi]
    mov     ecx,    [_hash_table-4+edi]
    mov     [_hash_table-8+edi],    eax
    mov     [_hash_table-4+edi],    edx
    mov     [_hash_table+edi],      ebx
    mov     [_hash_table+4+edi],    ecx
    jmp     edx
_B8:
    mov     edi,    eax
    and     edi,    0FFFFFFF8h
    inc     edi
    pusha
    call    _new_recompile_block
    test    eax,    eax
    popa
    je      _dyna_linker_ds
    ;pagefault
    and     eax,    0FFFFFFF8h
    mov     ecx,    080000008h    ;High bit set indicates pagefault in delay slot 
    mov     ebx,    eax
    sub     eax,    4
    jmp     exec_pagefault

global _jump_vaddr_eax

_jump_vaddr_eax:
    mov     edi,    eax
    jmp     _jump_vaddr_edi

global _jump_vaddr_ecx

_jump_vaddr_ecx:
    mov     edi,    ecx
    jmp     _jump_vaddr_edi

global _jump_vaddr_edx

_jump_vaddr_edx:
    mov     edi,    edx
    jmp     _jump_vaddr_edi

global _jump_vaddr_ebx

_jump_vaddr_ebx:
    mov     edi,    ebx
    jmp     _jump_vaddr_edi

global _jump_vaddr_ebp

_jump_vaddr_ebp:
    mov     edi,    ebp

global _jump_vaddr_edi

_jump_vaddr_edi:
    mov     eax,    edi

jump_vaddr:
    ;Check hash table
    shr     eax,    16
    xor     eax,    edi
    movzx   eax,    ax
    shl     eax,    4
    cmp     edi,    [_hash_table+eax]
    jne     _C2
_C1:
    mov     edi,    [_hash_table+4+eax]
    jmp     edi
_C2:
    cmp     edi,    [_hash_table+8+eax]
    lea     eax,    [8+eax]
    je      _C1
    ;No hit on hash table, call compiler
    add     esp,    -12
    push    edi
    mov     [_cycle_count],    esi    ;CCREG
    call    _get_addr
    mov     esi,    [_cycle_count]
    add     esp,    16
    jmp     eax

global _verify_code_ds

_verify_code_ds:
    mov     [_branch_target],    ebp

global _verify_code_vm

_verify_code_vm:
    ;eax = source (virtual _address)
    ;ebx = target
    ;ecx = length
    cmp     eax,    0C0000000h
    jl      _verify_code
    mov     edx,    eax
    lea     ebp,    [-1+eax+ecx*1]
    shr     edx,    12
    shr     ebp,    12
    mov     edi,    [_memory_map+edx*4]
    test    edi,    edi
    js      _D5
    lea     eax,    [eax+edi*4]
_D1:
    xor     edi,    [_memory_map+edx*4]
    shl     edi,    2
    jne     _D5
    mov     edi,    [_memory_map+edx*4]
    inc     edx
    cmp     edx,    ebp
    jbe     _D1

global _verify_code

_verify_code:
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
    mov     [_cycle_count],    esi
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
    mov     esi,    [_cycle_count]
    mov     ebp,    [_branch_target]
_D3:
    ret
_D4:
    mov     esi,    [_cycle_count]
_D5:
    mov     ebp,    [_branch_target]
    push    esi           ;for stack alignment, unused
    push    DWORD [8+esp]
    call    _get_addr
    add     esp,    16    ;pop stack
    jmp     eax

global _cc_interrupt

_cc_interrupt:
    add     esi,    [_last_count]
    add     esp,    -28                 ;Align stack
    mov     [_g_cp0_regs+36],    esi    ;Count
    shr     esi,    19
    mov     DWORD [_pending_exception],    0
    and     esi,    07fh
    cmp     DWORD [_restore_candidate+esi*4],    0
    jne     _E4
_E1:
    call    _gen_interupt
    mov     esi,    [_g_cp0_regs+36]
    mov     eax,    [_next_interupt]
    mov     ebx,    [_pending_exception]
    mov     ecx,    [_stop]
    add     esp,    28
    mov     [_last_count],    eax
    sub     esi,    eax
    test    ecx,    ecx
    jne     _E3
    test    ebx,    ebx
    jne     _E2
    ret
_E2:
    add     esp,    -8
    mov     edi,    [_pcaddr]
    mov     [_cycle_count],    esi    ;CCREG
    push    edi
    call    _get_addr_ht
    mov     esi,    [_cycle_count]
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
    mov     ebx,    [_restore_candidate+esi*4]
    mov     ebp,    esi
    mov     DWORD [_restore_candidate+esi*4],    0
    shl     ebp,    5
_E5:
    shr     ebx,    1
    jnc     _E6
    mov     [esp],    ebp
    call    _clean_blocks
_E6:
    inc     ebp
    test    ebp,    31
    jne     _E5
    jmp     _E1


global _do_interrupt


_do_interrupt:
    mov     edi,    [_pcaddr]
    add     esp,    -12
    push    edi
    call    _get_addr_ht
    add     esp,    16
    mov     esi,    [_g_cp0_regs+36]
    mov     ebx,    [_next_interupt]
    mov     [_last_count],    ebx
    sub     esi,    ebx
    add     esi,    2
    jmp     eax


global _fp_exception

_fp_exception:
    mov     edx,    01000002ch
_E7:
    mov     ebx,    [_g_cp0_regs+48]
    add     esp,    -12
    or      ebx,    2
    mov     [_g_cp0_regs+48],    ebx     ;Status
    mov     [_g_cp0_regs+52],    edx     ;Cause
    mov     [_g_cp0_regs+56],    eax     ;EPC
    push    080000180h
    call    _get_addr_ht
    add     esp,    16
    jmp     eax


global _fp_exception_ds

_fp_exception_ds:
    mov     edx,    09000002ch    ;Set high bit if delay slot
    jmp     _E7

global _jump_syscall

_jump_syscall:
    mov     edx,    020h
    mov     ebx,    [_g_cp0_regs+48]
    add     esp,    -12
    or      ebx,    2
    mov     [_g_cp0_regs+48],    ebx     ;Status
    mov     [_g_cp0_regs+52],    edx     ;Cause
    mov     [_g_cp0_regs+56],    eax     ;EPC
    push    080000180h
    call    _get_addr_ht
    add     esp,    16
    jmp     eax


global _jump_eret

_jump_eret:
    mov     ebx,    [_g_cp0_regs+48]        ;Status
    add     esi,    [_last_count]
    and     ebx,    0FFFFFFFDh
    mov     [_g_cp0_regs+36],    esi        ;Count
    mov     [_g_cp0_regs+48],    ebx        ;Status
    call    _check_interupt
    mov     eax,    [_next_interupt]
    mov     esi,    [_g_cp0_regs+36]
    mov     [_last_count],    eax
    sub     esi,    eax
    mov     eax,    [_g_cp0_regs+56]        ;EPC
    jns     _E11
_E8:
    mov     ebx,    248
    xor     edi,    edi
_E9:
    mov     ecx,    [_reg+ebx]
    mov     edx,    [_reg+4+ebx]
    sar     ecx,    31
    xor     edx,    ecx
    neg     edx
    adc     edi,    edi
    sub     ebx,    8
    jne     _E9
    mov     ecx,    [_hi+ebx]
    mov     edx,    [_hi+4+ebx]
    sar     ecx,    31
    xor     edx,    ecx
    jne     _E10
    mov     ecx,    [_lo+ebx]
    mov     edx,    [_lo+4+ebx]
    sar     ecx,    31
    xor     edx,    ecx
_E10:
    neg     edx
    adc     edi,    edi
    add     esp,    -8
    push    edi
    push    eax
    mov     [_cycle_count],    esi
    call    _get_addr_32
    mov     esi,    [_cycle_count]
    add     esp,    16
    jmp     eax
_E11:
    mov     [_pcaddr],    eax
    call    _cc_interrupt
    mov     eax,    [_pcaddr]
    jmp     _E8


global _new_dyna_start

_new_dyna_start:
    push    ebp
    push    ebx
    push    esi
    push    edi
    add     esp,    -8    ;align stack
    push    0a4000040h
    call    _new_recompile_block
    mov     edi,    DWORD [_next_interupt]
    mov     esi,    DWORD [_g_cp0_regs+36]
    mov     DWORD [_last_count],    edi
    sub     esi,    edi
    jmp     DWORD [_base_addr]


global _invalidate_block_eax

_invalidate_block_eax:
    push    eax
    push    ecx
    push    edx
    push    eax
    jmp     invalidate_block_call

global _invalidate_block_ecx

_invalidate_block_ecx:
    push    eax
    push    ecx
    push    edx
    push    ecx
    jmp     invalidate_block_call

global _invalidate_block_edx

_invalidate_block_edx:
    push    eax
    push    ecx
    push    edx
    push    edx
    jmp     invalidate_block_call

global _invalidate_block_ebx

_invalidate_block_ebx:
    push    eax
    push    ecx
    push    edx
    push    ebx
    jmp     invalidate_block_call

global _invalidate_block_ebp

_invalidate_block_ebp:
    push    eax
    push    ecx
    push    edx
    push    ebp
    jmp     invalidate_block_call

global _invalidate_block_esi

_invalidate_block_esi:
    push    eax
    push    ecx
    push    edx
    push    esi
    jmp     invalidate_block_call

global _invalidate_block_edi

_invalidate_block_edi:
    push    eax
    push    ecx
    push    edx
    push    edi

invalidate_block_call:
    call    _invalidate_block
    pop     eax ;Throw away
    pop     edx
    pop     ecx
    pop     eax
    ret

global _write_rdram_new

_write_rdram_new:
    mov     edi,    [_address]
    mov     ecx,    [_word]
    mov     [_rdram-0x80000000+edi],    ecx
    jmp     _E12


global _write_rdramb_new

_write_rdramb_new:
    mov     edi,    [_address]
    xor     edi,    3
    mov     cl,     BYTE [_cpu_byte]
    mov     BYTE [_rdram-0x80000000+edi],    cl
    jmp     _E12


global _write_rdramh_new

_write_rdramh_new:
    mov     edi,    [_address]
    xor     edi,    2
    mov     cx,     WORD [_hword]
    mov     WORD [_rdram-0x80000000+edi],    cx
    jmp     _E12


global _write_rdramd_new

_write_rdramd_new:
    mov     edi,    [_address]
    mov     ecx,    [dword+4]
    mov     edx,    [dword+0]
    mov     [_rdram-0x80000000+edi],      ecx
    mov     [_rdram-0x80000000+4+edi],    edx
    jmp     _E12


do_invalidate:
    mov     edi,    [_address]
    mov     ebx,    edi    ;Return ebx to caller
_E12:
    shr     edi,    12
    cmp     BYTE [_invalid_code+edi],    1
    je      _E13
    push    edi
    call    _invalidate_block
    pop     edi
_E13:
ret


global _read_nomem_new

_read_nomem_new:
    mov     edi,    [_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [_memory_map+edi*4]
    mov     eax,    08h
    test    edi,    edi
    js      tlb_exception
    mov     ecx,    [ebx+edi*4]
    mov     [_readmem_dword],    ecx
    ret


global _read_nomemb_new

_read_nomemb_new:
    mov     edi,    [_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [_memory_map+edi*4]
    mov     eax,    08h
    test    edi,    edi
    js      tlb_exception
    xor     ebx,    3
    movzx   ecx,    BYTE [ebx+edi*4]
    mov     [_readmem_dword],    ecx
    ret


global _read_nomemh_new

_read_nomemh_new:
    mov     edi,    [_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [_memory_map+edi*4]
    mov     eax,    08h
    test    edi,    edi
    js      tlb_exception
    xor     ebx,    2
    movzx   ecx,    WORD [ebx+edi*4]
    mov     [_readmem_dword],    ecx
    ret


global _read_nomemd_new

_read_nomemd_new:
    mov     edi,    [_address]
    mov     ebx,    edi
    shr     edi,    12
    mov     edi,    [_memory_map+edi*4]
    mov     eax,    08h
    test    edi,    edi
    js      tlb_exception
    mov     ecx,    [4+ebx+edi*4]
    mov     edx,    [ebx+edi*4]
    mov     [_readmem_dword],      ecx
    mov     [_readmem_dword+4],    edx
    ret


global _write_nomem_new

_write_nomem_new:
    call    do_invalidate
    mov     edi,    [_memory_map+edi*4]
    mov     ecx,    [_word]
    mov     eax,    0ch
    shl     edi,    2
    jc      tlb_exception
    mov     [ebx+edi],    ecx
    ret


global _write_nomemb_new

_write_nomemb_new:
    call    do_invalidate
    mov     edi,    [_memory_map+edi*4]
    mov     cl,     BYTE [_cpu_byte]
    mov     eax,    0ch
    shl     edi,    2
    jc      tlb_exception
    xor     ebx,    3
    mov     BYTE [ebx+edi],    cl
    ret


global _write_nomemh_new

_write_nomemh_new:
    call    do_invalidate
    mov     edi,    [_memory_map+edi*4]
    mov     cx,     WORD [_hword]
    mov     eax,    0ch
    shl     edi,    2
    jc      tlb_exception
    xor     ebx,    2
    mov     WORD [ebx+edi],    cx
    ret


global _write_nomemd_new

_write_nomemd_new:
    call    do_invalidate
    mov     edi,    [_memory_map+edi*4]
    mov     edx,    [dword+4]
    mov     ecx,    [dword+0]
    mov     eax,    0ch
    shl     edi,    2
    jc      tlb_exception
    mov     [ebx+edi],    edx
    mov     [4+ebx+edi],    ecx
    ret


tlb_exception:
    ;eax = cause
    ;ebx = _address
    ;ebp = instr addr + flags
    mov     ebp,    [024h+esp]
;Debug: 
    ;push    %ebp
    ;push    %ebx
    ;push    %eax
    ;call    tlb_debug
    ;pop     %eax
    ;pop     %ebx
    ;pop     %ebp
;end debug
    mov     esi,    [_g_cp0_regs+48]
    mov     ecx,    ebp
    mov     edx,    ebp
    mov     edi,    ebp
    shl     ebp,    31
    shr     ecx,    12
    or      eax,    ebp
    sar     ebp,    29
    and     edx,    0FFFFFFFCh
    mov     ecx,    [_memory_map+ecx*4]
    or      esi,    2
    mov     ecx,    [edx+ecx*4]
    add     edx,    ebp
    mov     [_g_cp0_regs+48],    esi    ;Status
    mov     [_g_cp0_regs+52],    eax    ;Cause
    mov     [_g_cp0_regs+56],    edx    ;EPC
    add     esp,    024h
    mov     edx,    06000022h
    mov     ebp,    ecx
    movsx   eax,    cx
    shr     ecx,    26
    shr     ebp,    21
    sub     ebx,    eax
    and     ebp,    01fh
    ror     edx,    cl
    mov     esi,    [_g_cp0_regs+16]
    cmovc   ebx,    [_reg+ebp*8]
    and     esi,    0FF80000Fh
    mov     [_reg+ebp*8],    ebx
    add     eax,    ebx
    sar     ebx,    31
    mov     [_g_cp0_regs+32],    eax    ;BadVAddr
    shr     eax,    9
    test    edi,    2
    cmove   ebx,    [_reg+4+ebp*8]
    add     esp,    -12
    and     eax,    0007FFFF0h
    mov     [_reg+4+ebp*8],    ebx
    push    080000180h
    or      esi,    eax
    mov     [_g_cp0_regs+16],    esi    ;Context
    call    _get_addr_ht
    add     esp,    16
    mov     edi,    DWORD [_next_interupt]
    mov     esi,    DWORD [_g_cp0_regs+36]    ;Count
    mov     DWORD [_last_count],    edi
    sub     esi,    edi
    jmp     eax


global _breakpoint

_breakpoint: