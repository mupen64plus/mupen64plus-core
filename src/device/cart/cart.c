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

#include "device/ri/rdram.h"

#include <stdint.h>
#include <string.h>

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
    NULL,
    process_cart_command,
    NULL
};

void init_cart(struct cart* cart,
               /* AF-RTC */
               void* af_rtc_clock, const struct clock_backend_interface* iaf_rtc_clock,
               /* cart ROM */
               uint8_t* rom, size_t rom_size,
               struct r4300_core* r4300,
               uint32_t* pi_status,
               struct rdram* rdram, const struct cic* cic,
               /* eeprom */
               uint16_t eeprom_type,
               void* eeprom_storage, const struct storage_backend_interface* ieeprom_storage,
               /* flashram */
               void* flashram_storage, const struct storage_backend_interface* iflashram_storage,
               /* sram */
               void* sram_storage, const struct storage_backend_interface* isram_storage)
{
    init_af_rtc(&cart->af_rtc,
        af_rtc_clock, iaf_rtc_clock);

    init_cart_rom(&cart->cart_rom,
        rom, rom_size,
        r4300,
        pi_status,
        rdram, cic);

    init_eeprom(&cart->eeprom,
        eeprom_type, eeprom_storage, ieeprom_storage);

    init_flashram(&cart->flashram,
        flashram_storage, iflashram_storage, (uint8_t*)rdram->dram);

    init_sram(&cart->sram,
        sram_storage, isram_storage);

    cart->use_flashram = 0;
}

void poweron_cart(struct cart* cart)
{
    poweron_af_rtc(&cart->af_rtc);
    poweron_cart_rom(&cart->cart_rom);
    poweron_flashram(&cart->flashram);
}

void read_cart_dom2(void* opaque, uint32_t address, uint32_t* value)
{
    struct cart* cart = (struct cart*)opaque;

    if ((cart->use_flashram == -1) || ((address & 0xffff) != 0))
    {
        DebugMessage(M64MSG_ERROR, "unknown read in read_cart_dom2()");
        return;
    }
    cart->use_flashram = 1;

    read_flashram_status(&cart->flashram, address, value);
}

void write_cart_dom2(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct cart* cart = (struct cart*)opaque;

    if ((cart->use_flashram == -1) || ((address & 0xffff) != 0))
    {
        DebugMessage(M64MSG_ERROR, "unknown write in write_cart_dom2_()");
        return;
    }
    cart->use_flashram = 1;

    write_flashram_command(&cart->flashram, address, value, mask);
}

void read_cart_dom2_dummy(void* opaque, uint32_t address, uint32_t* value)
{
    *value = 0;
}

void write_cart_dom2_dummy(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
}

unsigned int cart_dom2_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    struct cart* cart = (struct cart*)opaque;
    unsigned int cycles;

    if (cart->use_flashram != 1)
    {
        cycles = sram_dma_read(&cart->sram, dram, dram_addr, cart_addr, length);
        cart->use_flashram = -1;
    }
    else
    {
        cycles = flashram_dma_read(&cart->flashram, dram, dram_addr, cart_addr, length);
    }

    return cycles;
}

unsigned int cart_dom2_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    struct cart* cart = (struct cart*)opaque;
    unsigned int cycles;

    if (cart->use_flashram != 1)
    {
        cycles = sram_dma_write(&cart->sram, dram, dram_addr, cart_addr, length);
        cart->use_flashram = -1;
    }
    else
    {
        cycles = flashram_dma_write(&cart->flashram, dram, dram_addr, cart_addr, length);
    }

    return cycles;
}

unsigned int cart_dom3_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    return /* length / 8 */0x1000;
}

unsigned int cart_dom3_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    return /* length / 8 */0x1000;
}

