/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cart.c                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#include "cart.h"

#include "api/callbacks.h"
#include "api/m64p_types.h"

#include <stdint.h>
#include <string.h>

static void poweron_cart(void* jbd)
{
    struct cart* cart = (struct cart*)jbd;

    poweron_af_rtc(&cart->af_rtc);
}

static void process_cart_command(void* jbd,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf)
{
    struct cart* cart = (struct cart*)jbd;

    uint8_t cmd = tx_buf[0];

    switch (cmd)
    {
    case JCMD_RESET:
        /* TODO: perform internal reset */
    case JCMD_STATUS: {
        JOYBUS_CHECK_COMMAND_FORMAT(1, 3)

        /* set type and status */
        rx_buf[0] = (uint8_t)(cart->eeprom.type >> 0);
        rx_buf[1] = (uint8_t)(cart->eeprom.type >> 8);
        rx_buf[2] = 0x00;
    } break;

    case JCMD_EEPROM_READ: {
        JOYBUS_CHECK_COMMAND_FORMAT(2, 8)
        eeprom_read_block(&cart->eeprom, tx_buf[1], &rx_buf[0]);
    } break;

    case JCMD_EEPROM_WRITE: {
        JOYBUS_CHECK_COMMAND_FORMAT(10, 1)
        eeprom_write_block(&cart->eeprom, tx_buf[1], &tx_buf[2], &rx_buf[0]);
    } break;

    case JCMD_AF_RTC_STATUS: {
        JOYBUS_CHECK_COMMAND_FORMAT(1, 3)

        /* set type and status */
        rx_buf[0] = (uint8_t)(JDT_AF_RTC >> 0);
        rx_buf[1] = (uint8_t)(JDT_AF_RTC >> 8);
        rx_buf[2] = 0x00;
    } break;

    case JCMD_AF_RTC_READ: {
        JOYBUS_CHECK_COMMAND_FORMAT(2, 9)
        af_rtc_read_block(&cart->af_rtc, tx_buf[1], &rx_buf[0], &rx_buf[8]);
    } break;

    case JCMD_AF_RTC_WRITE: {
        JOYBUS_CHECK_COMMAND_FORMAT(10, 1)
        af_rtc_write_block(&cart->af_rtc, tx_buf[1], &tx_buf[2], &rx_buf[0]);
    } break;

    default:
        DebugMessage(M64MSG_WARNING, "cart: Unknown command %02x %02x %02x",
            *tx, *rx, cmd);
    }
}

const struct joybus_device_interface
    g_ijoybus_device_cart =
{
    poweron_cart,
    process_cart_command,
    NULL
};
