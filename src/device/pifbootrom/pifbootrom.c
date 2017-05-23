/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pifbootrom.c                                            *
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

#include "pifbootrom.h"

#include <stdint.h>
#include <string.h>

#include "api/m64p_types.h"
#include "device/device.h"
#include "device/ai/ai_controller.h"
#include "device/pi/pi_controller.h"
#include "device/r4300/mi_controller.h"
#include "device/r4300/r4300_core.h"
#include "device/rsp/rsp_core.h"
#include "device/si/si_controller.h"
#include "device/vi/vi_controller.h"
#include "main/rom.h"

static unsigned int get_tv_type(void)
{
    switch(ROM_PARAMS.systemtype)
    {
    default:
    case SYSTEM_NTSC: return 1;
    case SYSTEM_PAL: return 0;
    case SYSTEM_MPAL: return 2;
    }
}

void pifbootrom_hle_execute(struct device* dev)
{
    unsigned int rom_type = 0;              /* 0:Cart, 1:DD */
    unsigned int reset_type = 0;            /* 0:ColdReset, 1:NMI */
    unsigned int s7 = 0;                    /* ??? */
    unsigned int tv_type = get_tv_type();   /* 0:PAL, 1:NTSC, 2:MPAL */
    uint32_t bsd_dom1_config = *(uint32_t*)dev->pi.cart_rom.rom;

    int64_t* r4300_gpregs = r4300_regs(&dev->r4300);
    uint32_t* cp0_regs = r4300_cp0_regs(&dev->r4300.cp0);

    /* setup CP0 registers */
    cp0_regs[CP0_STATUS_REG] = 0x34000000;
    cp0_regs[CP0_CONFIG_REG] = 0x0006e463;

    /* stop RSP */
    dev->sp.regs[SP_STATUS_REG] = 1;
    dev->sp.regs2[SP_PC_REG] = 0;

    /* stop PI, configure ROM access */
    dev->pi.regs[PI_BSD_DOM1_LAT_REG] = (bsd_dom1_config      ) & 0xff;
    dev->pi.regs[PI_BSD_DOM1_PWD_REG] = (bsd_dom1_config >>  8) & 0xff;
    dev->pi.regs[PI_BSD_DOM1_PGS_REG] = (bsd_dom1_config >> 16) & 0x0f;
    dev->pi.regs[PI_BSD_DOM1_RLS_REG] = (bsd_dom1_config >> 20) & 0x03;
    dev->pi.regs[PI_STATUS_REG] = 0;

    /* mute sound */
    dev->ai.regs[AI_DRAM_ADDR_REG] = 0;
    dev->ai.regs[AI_LEN_REG] = 0;

    /* blank screen */
    dev->vi.regs[VI_V_INTR_REG] = 1023;
    dev->vi.regs[VI_CURRENT_REG] = 0;
    dev->vi.regs[VI_H_START_REG] = 0;

    /* clear RCP interrupts */
    dev->r4300.mi.regs[MI_INTR_REG] &= ~(MI_INTR_PI | MI_INTR_VI | MI_INTR_AI | MI_INTR_SP);

    /* copy IPL3 to dmem */
    memcpy((unsigned char*)dev->sp.mem+0x40, dev->pi.cart_rom.rom+0x40, 0xfc0);

    /* setup s3-s7 registers (needed by OS) */
    r4300_gpregs[19] = rom_type;     /* s3 */
    r4300_gpregs[20] = tv_type;      /* s4 */
    r4300_gpregs[21] = reset_type;   /* s5 */
    r4300_gpregs[22] = dev->si.pif.cic.seed;/* s6 */
    r4300_gpregs[23] = s7;           /* s7 */

    /* required by CIC x105 */
    dev->sp.mem[0x1000/4] = 0x3c0dbfc0;
    dev->sp.mem[0x1004/4] = 0x8da807fc;
    dev->sp.mem[0x1008/4] = 0x25ad07c0;
    dev->sp.mem[0x100c/4] = 0x31080080;
    dev->sp.mem[0x1010/4] = 0x5500fffc;
    dev->sp.mem[0x1014/4] = 0x3c0dbfc0;
    dev->sp.mem[0x1018/4] = 0x8da80024;
    dev->sp.mem[0x101c/4] = 0x3c0bb000;

    /* required by CIC x105 */
    r4300_gpregs[11] = INT64_C(0xffffffffa4000040); /* t3 */
    r4300_gpregs[29] = INT64_C(0xffffffffa4001ff0); /* sp */
    r4300_gpregs[31] = INT64_C(0xffffffffa4001550); /* ra */

    /* XXX: should prepare execution of IPL3 in DMEM here :
     * e.g. jump to 0xa4000040 */
    *r4300_cp0_last_addr(&dev->r4300.cp0) = 0xa4000040;
}
