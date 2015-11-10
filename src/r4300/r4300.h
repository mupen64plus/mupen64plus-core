/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300.h                                                 *
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

#ifndef M64P_R4300_R4300_H
#define M64P_R4300_R4300_H

#include <stdint.h>

#include "ops.h"
#include "r4300_core.h"
#include "cp0.h"

extern int stop, rompause;

struct r4300_registers {
    int64_t  gpr[32];      /* General-purpose registers */
    int64_t  hi;           /* Multiplier unit high bits */
    int64_t  lo;           /* Multiplier unit low bits */
    uint32_t cp0[CP0_REGS_COUNT]; /* Coprocessor 0 registers */
    float*   cp1_s[32];    /* Points into fpr_data for FPR singles */
    double*  cp1_d[32];    /* Points into fpr_data for FPR doubles */
    int64_t  fpr_data[32]; /* Raw data for floating-point registers */
    uint32_t fcr_0;        /* Floating-point control register 0 */
    uint32_t fcr_31;       /* Floating-point control register 31 */
    uint32_t ll_bit;       /* Whether the next SC can pair with an LL */
};

struct r4300_state {
    struct r4300_registers regs;
    /* Set to non-zero if the currently-executing opcode is in the delay slot
     * of a branch. Exception creation code uses this. */
    uint32_t delay_slot;
    /* Set to the next value of regs.cp0[CP0_COUNT_REG] that will trigger an
     * interrupt on the CPU. */
    uint32_t next_interrupt;
    /* Set to the (N64) address to be read or written by a memory accessor. */
    uint32_t access_addr;
    /* Set to the (native) address of a 64-bit value that will receive data
     * read by memory accessors. */
    uint64_t* read_dest;
    /* Set to a value to be written by a memory accessor. */
    union {
        uint64_t d;
        uint32_t w;
        uint16_t h;
        uint8_t  b;
    } write;
};

extern struct r4300_state g_state;

/* XXX: The above types are required by [arch]/assemble.h, included by
 * recomp.h, because they read g_state.
 * In turn, recomp.h defines precomp_instr, which is used as PC's type below.
 */
#include "recomp.h"
extern precomp_instr *PC;

extern long long int local_rs;
extern uint32_t skip_jump;
extern unsigned int dyna_interp;
extern unsigned int r4300emu;
extern uint32_t last_addr;
#define COUNT_PER_OP_DEFAULT 2
extern unsigned int count_per_op;
extern cpu_instruction_table current_instruction_table;

void r4300_reset_hard(void);
void r4300_reset_soft(void);
void r4300_execute(void);

// r4300 emulators
#define CORE_PURE_INTERPRETER 0
#define CORE_INTERPRETER      1
#define CORE_DYNAREC          2
#define CORE_TRACE_JIT        3

#endif /* M64P_R4300_R4300_H */

