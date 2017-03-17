/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - new_dynarec.h                                           *
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

#ifndef M64P_DEVICE_R4300_NEW_DYNAREC_H
#define M64P_DEVICE_R4300_NEW_DYNAREC_H

#include <stddef.h>
#include <stdint.h>

#define NEW_DYNAREC_X86 1
#define NEW_DYNAREC_AMD64 2
#define NEW_DYNAREC_ARM 3

struct r4300_core;

extern int pcaddr;
extern int pending_exception;
extern unsigned int stop_after_jal;
extern unsigned int using_tlb;

#if NEW_DYNAREC == NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
extern uint32_t g_dev_mem_address;
extern uint8_t g_dev_mem_wbyte;
extern uint16_t g_dev_mem_whword;
extern uint32_t g_dev_mem_wword;
extern uint64_t g_dev_mem_wdword;

extern int64_t g_dev_r4300_regs[32];
extern int64_t g_dev_r4300_hi;
extern int64_t g_dev_r4300_lo;
extern struct precomp_instr* g_dev_r4300_pc;
extern int g_dev_r4300_stop;

extern uint32_t g_dev_r4300_cp0_regs[32];
extern unsigned int g_dev_r4300_cp0_next_interrupt;

extern float* g_dev_r4300_cp1_regs_simple[32];
extern double* g_dev_r4300_cp1_regs_double[32];
extern uint32_t g_dev_r4300_cp1_fcr0;
extern uint32_t g_dev_r4300_cp1_fcr31;
#endif

void invalidate_all_pages(void);
void invalidate_cached_code_new_dynarec(struct r4300_core* r4300, uint32_t address, size_t size);
void new_dynarec_init(void);
void new_dyna_start(void);
void new_dynarec_cleanup(void);

#endif /* M64P_DEVICE_R4300_NEW_DYNAREC_H */
