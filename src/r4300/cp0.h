/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp0.h                                                   *
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

#ifndef M64P_R4300_CP0_H
#define M64P_R4300_CP0_H

#include <stdint.h>

enum
{
    CP0_STATUS_IE   = 0x00000001,
    CP0_STATUS_EXL  = 0x00000002,
    CP0_STATUS_ERL  = 0x00000004,
    /* Execution modes */
    CP0_STATUS_MODE_K    = 0   << 3,
    CP0_STATUS_MODE_S    = 1   << 3,
    CP0_STATUS_MODE_U    = 2   << 3,
    CP0_STATUS_MODE_MASK = 0x3 << 3,

    CP0_STATUS_UX   = 0x00000020,
    CP0_STATUS_SX   = 0x00000040,
    CP0_STATUS_KX   = 0x00000080,
    CP0_STATUS_IM0  = 0x00000100,
    CP0_STATUS_IM1  = 0x00000200,
    CP0_STATUS_IM2  = 0x00000400,
    CP0_STATUS_IM3  = 0x00000800,
    CP0_STATUS_IM4  = 0x00001000,
    CP0_STATUS_IM5  = 0x00002000,
    CP0_STATUS_IM6  = 0x00004000,
    CP0_STATUS_IM7  = 0x00008000,
    /* bit 16 and 17 are left for compatibility */
    CP0_STATUS_CH   = 0x00040000,
    /* bit 19 is zero */
    CP0_STATUS_SR   = 0x00100000,
    CP0_STATUS_TS   = 0x00200000,
    CP0_STATUS_BEV  = 0x00400000,
    CP0_STATUS_RSVD = 0x00800000,
    CP0_STATUS_ITS  = 0x01000000,
    CP0_STATUS_RE   = 0x02000000,
    CP0_STATUS_FR   = 0x04000000,
    CP0_STATUS_RP   = 0x08000000,
    CP0_STATUS_CU0  = 0x10000000,
    CP0_STATUS_CU1  = 0x20000000,
    CP0_STATUS_CU2  = 0x40000000,
    CP0_STATUS_CU3  = 0x80000000,
};

enum
{
    /* Execution Codes */
    CP0_CAUSE_EXCCODE_INT   = 0    << 2,
    CP0_CAUSE_EXCCODE_MOD   = 1    << 2,
    CP0_CAUSE_EXCCODE_TLBL  = 2    << 2,
    CP0_CAUSE_EXCCODE_TLBS  = 3    << 2,
    CP0_CAUSE_EXCCODE_ADEL  = 4    << 2,
    CP0_CAUSE_EXCCODE_ADES  = 5    << 2,
    CP0_CAUSE_EXCCODE_IBE   = 6    << 2,
    CP0_CAUSE_EXCCODE_DBE   = 7    << 2,
    CP0_CAUSE_EXCCODE_SYS   = 8    << 2,
    CP0_CAUSE_EXCCODE_BP    = 9    << 2,
    CP0_CAUSE_EXCCODE_RI    = 10   << 2,
    CP0_CAUSE_EXCCODE_CPU   = 11   << 2,
    CP0_CAUSE_EXCCODE_OV    = 12   << 2,
    CP0_CAUSE_EXCCODE_TR    = 13   << 2,
    /* 14 is reserved */
    CP0_CAUSE_EXCCODE_FPE   = 15   << 2,
    /* 16-22 are reserved */
    CP0_CAUSE_EXCCODE_WATCH = 23   << 2,
    /* 24-31 are reserved */
    CP0_CAUSE_EXCCODE_MASK  = 0x1f << 2,

    /* Interrupt Pending */
    CP0_CAUSE_IP0  = 0x00000100,
    CP0_CAUSE_IP1  = 0x00000200,
    CP0_CAUSE_IP2  = 0x00000400,
    CP0_CAUSE_IP3  = 0x00000800,
    CP0_CAUSE_IP4  = 0x00001000,
    CP0_CAUSE_IP5  = 0x00002000,
    CP0_CAUSE_IP6  = 0x00004000,
    CP0_CAUSE_IP7  = 0x00008000,

    CP0_CAUSE_CE1 = 0x10000000,
    CP0_CAUSE_BD  = 0x80000000,
};

enum r4300_cp0_registers
{
    CP0_INDEX_REG,
    CP0_RANDOM_REG,
    CP0_ENTRYLO0_REG,
    CP0_ENTRYLO1_REG,
    CP0_CONTEXT_REG,
    CP0_PAGEMASK_REG,
    CP0_WIRED_REG,
    /* 7 is unused */
    CP0_BADVADDR_REG = 8,
    CP0_COUNT_REG,
    CP0_ENTRYHI_REG,
    CP0_COMPARE_REG,
    CP0_STATUS_REG,
    CP0_CAUSE_REG,
    CP0_EPC_REG,
    CP0_PREVID_REG,
    CP0_CONFIG_REG,
    CP0_LLADDR_REG,
    CP0_WATCHLO_REG,
    CP0_WATCHHI_REG,
    CP0_XCONTEXT_REG,
    /* 21 - 27 are unused */
    CP0_TAGLO_REG = 28,
    CP0_TAGHI_REG,
    CP0_ERROREPC_REG,
    /* 31 is unused */
    CP0_REGS_COUNT = 32
};

uint32_t* r4300_cp0_regs(void);

void cp0_update_count(void);

#endif /* M64P_R4300_CP0_H */

