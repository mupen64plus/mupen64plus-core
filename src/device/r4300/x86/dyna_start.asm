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

%define g_dev_r4300_save_ebp       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_ebp)
%define g_dev_r4300_save_esp       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_esp)
%define g_dev_r4300_save_ebx       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_ebx)
%define g_dev_r4300_save_esi       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_esi)
%define g_dev_r4300_save_edi       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_edi)
%define g_dev_r4300_save_eip       (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_save_eip)
%define g_dev_r4300_return_address (g_dev + offsetof_struct_device_r4300 + offsetof_struct_r4300_core_return_address)

cglobal dyna_start

cextern g_dev

section .bss
align 4

section .rodata
section .text

dyna_start:
    mov  [g_dev_r4300_save_ebp], ebp
    mov  [g_dev_r4300_save_esp], esp
    mov  [g_dev_r4300_save_ebx], ebx
    mov  [g_dev_r4300_save_esi], esi
    mov  [g_dev_r4300_save_edi], edi
    call point1
    jmp  point2
point1:
    pop  eax
    mov  [g_dev_r4300_save_eip], eax
    mov  eax, [esp+4]
    sub  esp, 0x10
    and  esp, 0xfffffff0
    mov  [g_dev_r4300_return_address], esp
    sub  DWORD [g_dev_r4300_return_address], 4
    call eax
point2:
    mov  ebp, [g_dev_r4300_save_ebp]
    mov  esp, [g_dev_r4300_save_esp]
    mov  ebx, [g_dev_r4300_save_ebx]
    mov  esi, [g_dev_r4300_save_esi]
    mov  edi, [g_dev_r4300_save_edi]
    
    mov  DWORD [g_dev_r4300_save_ebp], 0
    mov  DWORD [g_dev_r4300_save_esp], 0
    mov  DWORD [g_dev_r4300_save_ebx], 0
    mov  DWORD [g_dev_r4300_save_esi], 0
    mov  DWORD [g_dev_r4300_save_edi], 0
    mov  DWORD [g_dev_r4300_save_eip], 0
    ret
