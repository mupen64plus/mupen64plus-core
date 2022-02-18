/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - joybus.h                                                *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2017 Bobby Smiles                                       *
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

#ifndef M64P_BACKENDS_API_JOYBUS_H
#define M64P_BACKENDS_API_JOYBUS_H

#include <stdint.h>

enum joybus_commands
{
    JCMD_STATUS = 0x00,
    JCMD_CONTROLLER_READ = 0x01,
    JCMD_PAK_READ = 0x02,
    JCMD_PAK_WRITE = 0x03,
    JCMD_EEPROM_READ = 0x04,
    JCMD_EEPROM_WRITE = 0x05,
    JCMD_AF_RTC_STATUS = 0x06,
    JCMD_AF_RTC_READ = 0x07,
    JCMD_AF_RTC_WRITE = 0x08,
    JCMD_VRU_READ = 0x09,
    JCMD_VRU_WRITE = 0x0A,
    JCMD_VRU_READ_STATUS = 0x0B,
    JCMD_VRU_WRITE_CONFIG = 0x0C,
    JCMD_VRU_WRITE_INIT = 0x0D,
    JCMD_RESET = 0xff,
};

enum joybus_device_types
{
    JDT_NONE             = 0x0000,
    JDT_JOY_ABS_COUNTERS = 0x0001,  /* joystick with absolute coordinates */
    JDT_JOY_REL_COUNTERS = 0x0002,  /* joystick with relative coordinates (= mouse) */
    JDT_JOY_PORT         = 0x0004,  /* has port for external paks */
    JDT_VRU              = 0x0100,  /* VRU */
    JDT_AF_RTC           = 0x1000,  /* RTC */
    JDT_EEPROM_4K        = 0x8000,  /* 4k EEPROM */
    JDT_EEPROM_16K       = 0xc000,  /* 16k EEPROM */
};

/* snippet which helps validate command format */
#define JOYBUS_CHECK_COMMAND_FORMAT(expected_tx, expected_rx) \
    if (*tx != expected_tx || *rx != expected_rx) { \
        DebugMessage(M64MSG_WARNING, "Unexpected command format %02x %02x %02x ", \
            *tx, *rx, cmd); \
        *rx |= 0x40; \
        break; \
    }



struct joybus_device_interface
{
    /* Optional. Called at device poweron.
     * Allow the joybus device to initialze itself.
     */
    void (*poweron)(void* jbd);

    /* Required. Perform command processing.
     */
    void (*process)(void* jbd,
        const uint8_t* tx, const uint8_t* tx_buf,
        uint8_t* rx, uint8_t* rx_buf);

    /* Optional. Called after channel setup.
     */
    void (*post_setup)(void* jbd,
        uint8_t* tx, const uint8_t* tx_buf,
        const uint8_t* rx, const uint8_t* rx_buf);
};

#endif
