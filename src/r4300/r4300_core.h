/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300_core.h                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#ifndef M64P_R4300_R4300_CORE_H
#define M64P_R4300_R4300_CORE_H

#include <stddef.h>
#include <stdint.h>

#if defined(PROFILE_R4300)
#include <stdio.h>
#endif

#include "cp0.h"
#include "cp1.h"
#include "mi_controller.h"

#include "ops.h" /* for cpu_instruction_table */
#include "recomp_types.h" /* for precomp_instr */

struct cached_interp
{
    char invalid_code[0x100000];
    struct precomp_block* blocks[0x100000];
    struct precomp_block* actual;
    unsigned int jump_to_address;
};

enum {
    EMUMODE_PURE_INTERPRETER = 0,
    EMUMODE_INTERPRETER      = 1,
    EMUMODE_DYNAREC          = 2,
};


struct r4300_core
{
    int64_t regs[32];
    int64_t hi;
    int64_t lo;
    unsigned int llbit;

    struct precomp_instr* pc;
    unsigned int delay_slot;
    long long int local_rs;
    uint32_t skip_jump;
    int stop;
    unsigned int dyna_interp;
    struct cpu_instruction_table current_instruction_table;

    /* When reset_hard_job is set, next interrupt will cause hard reset */
    int reset_hard_job;

    /* from pure_interp.c */
    struct precomp_instr interp_PC;

    /* from cached_interp.c.
     * XXX: more work is needed to correctly encapsulate these */
    struct cached_interp cached_interp;

    /* from recomp.c.
     * XXX: more work is needed to correctly encapsulate these */
    struct {
        int init_length;
        struct precomp_instr* dst;          /* destination structure for the recompiled instruction */
        int code_length;                    /* current real recompiled code length */
        int max_code_length;                /* current recompiled code's buffer length */
        unsigned char **inst_pointer;       /* output buffer for recompiled code */
        struct precomp_block *dst_block;    /* the current block that we are recompiling */
        uint32_t src;                       /* the current recompiled instruction */
        int fast_memory;
        int no_compiled_jump;               /* use cached interpreter instead of recompiler for jumps */
        void (*recomp_func)(void);          /* pointer to the dynarec's generator
                                               function for the latest decoded opcode */
        const uint32_t *SRC;                /* currently recompiled instruction in the input stream */
        int check_nop;                      /* next instruction is nop ? */
        int delay_slot_compiled;

#if defined(PROFILE_R4300)
        FILE* pfProfile;
#endif
    } recomp;

    unsigned int emumode;

    struct cp0 cp0;

    struct cp1 cp1;

    struct mi_controller mi;
};

void init_r4300(struct r4300_core* r4300, unsigned int emumode, unsigned int count_per_op, int no_compiled_jump);
void poweron_r4300(struct r4300_core* r4300);

int64_t* r4300_regs(void);
int64_t* r4300_mult_hi(void);
int64_t* r4300_mult_lo(void);
unsigned int* r4300_llbit(void);
uint32_t* r4300_pc(void);

unsigned int get_r4300_emumode(void);

/* Allow cached/dynarec r4300 implementations to invalidate
 * their cached code at [address, address+size]
 *
 * If size == 0, r4300 implementation should invalidate
 * all cached code.
 */
void invalidate_r4300_cached_code(uint32_t address, size_t size);


/* Jump to the given address. This works for all r4300 emulator, but is slower.
 * Use this for common code which can be executed from any r4300 emulator. */
void generic_jump_to(unsigned int address);

void savestates_load_set_pc(uint32_t pc);

#endif
