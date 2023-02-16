/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp2.h                                                   *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#ifndef M64P_DEVICE_R4300_CP2_H
#define M64P_DEVICE_R4300_CP2_H

#include <stdint.h>
#include "osal/preproc.h"
#include "new_dynarec/new_dynarec.h"

typedef union {
    int64_t  dword;
    double   float64;
    float    float32[2];
} cp2_reg;

struct cp2
{
    cp2_reg regs[32];

#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    uint32_t fcr0;
    uint32_t fcr31;

    float* regs_simple[32];
    double* regs_double[32];
#endif

    /* This is the x86 version of the rounding mode contained in FCR31.
     * It should not really be here. Its size should also really be uint16_t,
     * because FLDCW (Floating-point LoaD Control Word) loads 16-bit control
     * words. However, x86/gcop1.c and x86-64/gcop1.c update this variable
     * using 32-bit stores. */
    uint32_t rounding_mode;

#ifdef OSAL_SSE
    uint32_t flush_mode;
#endif

#ifdef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    struct new_dynarec_hot_state* new_dynarec_hot_state;
#endif
};

#ifndef NEW_DYNAREC
#define R4300_CP2_REGS_S_OFFSET (\
    offsetof(struct r4300_core, cp2) + \
    offsetof(struct cp2, regs_simple))
#else
#define R4300_CP2_REGS_S_OFFSET (\
    offsetof(struct r4300_core, new_dynarec_hot_state) + \
    offsetof(struct new_dynarec_hot_state, CP2_regs_simple))
#endif

#ifndef NEW_DYNAREC
#define R4300_CP2_REGS_D_OFFSET (\
    offsetof(struct r4300_core, cp2) + \
    offsetof(struct cp2, regs_double))
#else
#define R4300_CP2_REGS_D_OFFSET (\
    offsetof(struct r4300_core, new_dynarec_hot_state) + \
    offsetof(struct new_dynarec_hot_state, CP2_regs_double))
#endif

#ifndef NEW_DYNAREC
#define R4300_CP2_FCR0_OFFSET (\
    offsetof(struct r4300_core, cp2) + \
    offsetof(struct cp2, fcr0))
#else
#define R4300_CP2_FCR0_OFFSET (\
    offsetof(struct r4300_core, new_dynarec_hot_state) + \
    offsetof(struct new_dynarec_hot_state, cp2_fcr0))
#endif

#ifndef NEW_DYNAREC
#define R4300_CP2_FCR31_OFFSET (\
    offsetof(struct r4300_core, cp2) + \
    offsetof(struct cp2, fcr31))
#else
#define R4300_CP2_FCR31_OFFSET (\
    offsetof(struct r4300_core, new_dynarec_hot_state) + \
    offsetof(struct new_dynarec_hot_state, cp2_fcr31))
#endif

void init_cp2(struct cp2* cp2, struct new_dynarec_hot_state* new_dynarec_hot_state);
void poweron_cp2(struct cp2* cp2);

cp2_reg* r4300_cp2_regs(struct cp2* cp2);
float** r4300_cp2_regs_simple(struct cp2* cp2);
double** r4300_cp2_regs_double(struct cp2* cp2);

uint32_t* r4300_cp2_fcr0(struct cp2* cp2);
uint32_t* r4300_cp2_fcr31(struct cp2* cp2);

void set_cp2_fpr_pointers(struct cp2* cp2, uint32_t newStatus);

#endif /* M64P_DEVICE_R4300_CP2_H */

