/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pif.h                                                   *
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

#ifndef M64P_DEVICE_SI_PIF_H
#define M64P_DEVICE_SI_PIF_H

#include <stdint.h>

#include "af_rtc.h"
#include "cic.h"
#include "eeprom.h"
#include "game_controller.h"

enum { GAME_CONTROLLERS_COUNT = 4 };

struct si_controller;

enum { PIF_RAM_SIZE = 0x40 };
enum { PIF_CHANNELS_COUNT = 5 };

enum pif_commands
{
    PIF_CMD_STATUS = 0x00,
    PIF_CMD_CONTROLLER_READ = 0x01,
    PIF_CMD_PAK_READ = 0x02,
    PIF_CMD_PAK_WRITE = 0x03,
    PIF_CMD_EEPROM_READ = 0x04,
    PIF_CMD_EEPROM_WRITE = 0x05,
    PIF_CMD_AF_RTC_STATUS = 0x06,
    PIF_CMD_AF_RTC_READ = 0x07,
    PIF_CMD_AF_RTC_WRITE = 0x08,
    PIF_CMD_RESET = 0xff,
};

enum pif_peripheral_device_types
{
    PIF_PDT_JOY_ABS_COUNTERS = 0x1,     /* joystick with absolute coordinates */
    PIF_PDT_JOY_REL_COUNTERS = 0x2,     /* joystick with relative coordinates (= mouse) */
    PIF_PDT_JOY_PORT         = 0x4,     /* has port for external paks */
    PIF_PDT_EEPROM_4K        = 0x200,   /* 4k EEPROM */
    PIF_PDT_EEPROM_16K       = 0x800,   /* 16k EEPROM */
    PIF_PDT_AF_RTC           = 0x1000,  /* RTC */
};

/* snippet which helps validate command format */
#define PIF_CHECK_COMMAND_FORMAT(expected_tx, expected_rx) \
    if (*tx != expected_tx || *rx != expected_rx) { \
        DebugMessage(M64MSG_WARNING, "Unexpected command format %02x %02x %02x ", \
            *tx, *rx, cmd); \
        *rx |= 0x40; \
        break; \
    }


struct pif_channel_device
{
    void* opaque;
    void (*process)(void* opaque,
        const uint8_t* tx, const uint8_t* tx_buf,
        uint8_t* rx, uint8_t* rx_buf);

    void (*post_setup)(void* opaque,
        uint8_t* tx, const uint8_t* tx_buf,
        const uint8_t* rx, const uint8_t* rx_buf);
};

struct pif_channel
{
    struct pif_channel_device device;

    uint8_t* tx;
    uint8_t* tx_buf;
    uint8_t* rx;
    uint8_t* rx_buf;
};

struct pif
{
    uint8_t ram[PIF_RAM_SIZE];

    struct pif_channel channels[PIF_CHANNELS_COUNT];

    struct game_controller controllers[GAME_CONTROLLERS_COUNT];
    struct eeprom eeprom;
    struct af_rtc af_rtc;

    struct cic cic;
};

static uint32_t pif_ram_address(uint32_t address)
{
    return ((address & 0xfffc) - 0x7c0);
}


void init_pif(struct pif* pif,
    const struct pif_channel_device* pif_channel_devices,
    struct controller_input_backend* cins,
    struct storage_backend* mpk_storages,
    struct rumble_backend* rumbles,
    struct gb_cart* gb_carts,
    uint16_t eeprom_id,
    struct storage_backend* eeprom_storage,
    struct clock_backend* clock,
    const uint8_t* ipl3);

void poweron_pif(struct pif* pif);

int read_pif_ram(void* opaque, uint32_t address, uint32_t* value);
int write_pif_ram(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void process_pif_ram(struct si_controller* si);
void update_pif_ram(struct si_controller* si);

void process_cart_command(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf);

#endif

