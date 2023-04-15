/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp2.c                                                   *
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

#include <stdint.h>
#include <string.h>

#include "cp0.h"
#include "cp1.h"
#include "cp2.h"

#include "new_dynarec/new_dynarec.h"

#define FCR31_FS_BIT UINT32_C(0x2000000)

void init_cp2(struct cp2* cp2, struct new_dynarec_hot_state* new_dynarec_hot_state)
{
#ifdef NEW_DYNAREC
    cp2->new_dynarec_hot_state = new_dynarec_hot_state;
#endif
}

void poweron_cp2(struct cp2* cp2)
{
    *r4300_cp2_latch(cp2) = 0;
}

uint64_t* r4300_cp2_latch(struct cp2* cp2)
{
#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    return &cp2->latch;
#else
    return &cp2->new_dynarec_hot_state->cp2_latch;
#endif
}

