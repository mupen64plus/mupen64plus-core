/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - asm_defines.c                                           *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2016 Bobby Smiles                                       *
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

/**
 * This file is to be compiled with the same compilation flags as the
 * mupen64plus-core, but without LTO / Global Optimizations
 * (those tends to inhibit effective creation of required symbols).
 * It's purpose is to help generate asm_defines headers
 * suitable for inclusion in assembly files.
 * This allow to effectively share struct definitions between C and assembly
 * files.
 */

#include "main/device.h"
#include "memory/memory.h"
#include "r4300/r4300_core.h"
#include "ri/ri_controller.h"



#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE*)0)->MEMBER)

/* Internally used to define a dummy array named "sym" and whose size is "val" bytes.
 * This eases extraction of such information using objdump/dumpbin/nm tools.
 */
#define _DEFINE(sym, val) const char sym[val];

/* Export member m of structure s.
 * Suitable parsing of corresponding object file (objdump/dumpbin/awk)
 * can be used to generate header suitable for inclusion in assembly files.
 */
#define DEFINE(s, m) \
    _DEFINE(offsetof_struct_##s##_##m, offsetof(struct s, m));


/* Structure members definitions */
DEFINE(device, r4300);

DEFINE(r4300_core, regs);
DEFINE(r4300_core, hi);
DEFINE(r4300_core, lo);

DEFINE(r4300_core, stop);

#if defined(__x86_64__)
DEFINE(r4300_core, save_rsp);
DEFINE(r4300_core, save_rip);
#endif
DEFINE(r4300_core, return_address);

DEFINE(r4300_core, cp0);
DEFINE(cp0, regs);
DEFINE(cp0, next_interrupt);
DEFINE(cp0, last_addr);
DEFINE(cp0, count_per_op);
DEFINE(cp0, tlb);

DEFINE(tlb, entries);
DEFINE(tlb, LUT_r);
DEFINE(tlb, LUT_w);

DEFINE(r4300_core, cached_interp);
DEFINE(cached_interp, invalid_code);

DEFINE(device, mem);

DEFINE(memory, wbyte);
DEFINE(memory, whword);
DEFINE(memory, wword);
DEFINE(memory, wdword);
DEFINE(memory, address);

DEFINE(device, ri);
DEFINE(ri_controller, rdram);
DEFINE(rdram, dram);
