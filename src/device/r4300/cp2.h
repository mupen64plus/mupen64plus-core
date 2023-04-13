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

struct cp2
{
    uint64_t latch;

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

void init_cp2(struct cp2* cp2, struct new_dynarec_hot_state* new_dynarec_hot_state);
void poweron_cp2(struct cp2* cp2);

uint64_t* r4300_cp2_latch(struct cp2* cp2);

#endif /* M64P_DEVICE_R4300_CP2_H */

