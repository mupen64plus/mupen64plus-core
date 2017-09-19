/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pif.c                                                   *
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

#include "pif.h"

#include <stdint.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_plugin.h"
#include "api/m64p_types.h"
#include "backends/controller_input_backend.h"
#include "backends/rumble_backend.h"
#include "backends/storage_backend.h"
#include "device/gb/gb_cart.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/si/n64_cic_nus_6105.h"
#include "device/si/si_controller.h"
#include "plugin/plugin.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//#define DEBUG_PIF
#ifdef DEBUG_PIF
void print_pif(struct pif* pif)
{
    int i;
    for (i=0; i<(64/8); i++) {
        DebugMessage(M64MSG_INFO, "%02" PRIX8 " %02" PRIX8 " %02" PRIX8 " %02" PRIX8 " | %02" PRIX8 " %02" PRIX8 " %02" PRIX8 " %02" PRIX8,
                     pif->ram[i*8+0], pif->ram[i*8+1],pif->ram[i*8+2], pif->ram[i*8+3],
                     pif->ram[i*8+4], pif->ram[i*8+5],pif->ram[i*8+6], pif->ram[i*8+7]);
    }

    for(i = 0; i < PIF_CHANNELS_COUNT; ++i) {
        if (pif->channels[i].tx != NULL) {
            DebugMessage(M64MSG_INFO, "Channel %u, tx=%02x rx=%02x cmd=%02x",
                i,
                *(pif->channels[i].tx),
                *(pif->channels[i].rx),
                pif->channels[i].tx_buf[0]);
        }
    }
}
#endif

void process_cart_command(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf)
{
    struct pif* pif = (struct pif*)opaque;

    uint8_t cmd = tx_buf[0];

    switch (cmd)
    {
    case PIF_CMD_STATUS: {
        PIF_CHECK_COMMAND_FORMAT(1, 3)

        /* set type and status */
        rx_buf[0] = (uint8_t)(pif->eeprom.type >> 0);
        rx_buf[1] = (uint8_t)(pif->eeprom.type >> 8);
        rx_buf[2] = 0x00;
    } break;

    case PIF_CMD_EEPROM_READ: {
        PIF_CHECK_COMMAND_FORMAT(2, 8)
        eeprom_read_block(&pif->eeprom, tx_buf[1], &rx_buf[0]);
    } break;

    case PIF_CMD_EEPROM_WRITE: {
        PIF_CHECK_COMMAND_FORMAT(10, 1)
        eeprom_write_block(&pif->eeprom, tx_buf[1], &tx_buf[2], &rx_buf[0]);
    } break;

    case PIF_CMD_AF_RTC_STATUS: {
        PIF_CHECK_COMMAND_FORMAT(1, 3)

        /* set type and status */
        rx_buf[0] = (uint8_t)(PIF_PDT_AF_RTC >> 0);
        rx_buf[1] = (uint8_t)(PIF_PDT_AF_RTC >> 8);
        rx_buf[2] = 0x00;
    } break;

    case PIF_CMD_AF_RTC_READ: {
        PIF_CHECK_COMMAND_FORMAT(2, 9)
        af_rtc_read_block(&pif->af_rtc, tx_buf[1], &rx_buf[0], &rx_buf[8]);
    } break;

    case PIF_CMD_AF_RTC_WRITE: {
        PIF_CHECK_COMMAND_FORMAT(10, 1)
        af_rtc_write_block(&pif->af_rtc, tx_buf[1], &tx_buf[2], &rx_buf[0]);
    } break;

    default:
        DebugMessage(M64MSG_WARNING, "cart: Unknown command %02x %02x %02x",
            *tx, *rx, cmd);
    }
}


static void init_pif_channel(struct pif_channel* channel,
    const struct pif_channel_device* device)
{
    memcpy(&channel->device, device, sizeof(*device));
}

static void process_channel(struct pif_channel* channel)
{
    /* don't process channel if it has been disabled */
    if (channel->tx == NULL) {
        return;
    }

    channel->device.process(channel->device.opaque,
        channel->tx, channel->tx_buf,
        channel->rx, channel->rx_buf);
}

static void post_setup_channel(struct pif_channel* channel)
{
    if (channel->device.post_setup == NULL) {
        return;
    }

    channel->device.post_setup(channel->device.opaque,
        channel->tx, channel->tx_buf,
        channel->rx, channel->rx_buf);
}

static void disable_pif_channel(struct pif_channel* channel)
{
    channel->tx = NULL;
    channel->rx = NULL;
    channel->tx_buf = NULL;
    channel->rx_buf = NULL;
}

static size_t setup_pif_channel(struct pif_channel* channel, uint8_t* buf)
{
    uint8_t tx = buf[0] & 0x3f;
    uint8_t rx = buf[1] & 0x3f;

    /* XXX: check out of bounds accesses */

    channel->tx = buf;
    channel->rx = buf + 1;
    channel->tx_buf = buf + 2;
    channel->rx_buf = buf + 2 + tx;

    post_setup_channel(channel);

    return 2 + tx + rx;
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
    const uint8_t* ipl3)
{
    size_t i;

    for(i = 0; i < PIF_CHANNELS_COUNT; ++i) {
        init_pif_channel(&pif->channels[i], &pif_channel_devices[i]);
    }

    for(i = 0; i < GAME_CONTROLLERS_COUNT; ++i) {
        init_game_controller(&pif->controllers[i],
                &cins[i],
                &mpk_storages[i],
                &rumbles[i],
                (gb_carts[i].rom.data != NULL) ? &gb_carts[i] : NULL);
    }

    init_eeprom(&pif->eeprom, eeprom_id, eeprom_storage);
    init_af_rtc(&pif->af_rtc, clock);

    init_cic_using_ipl3(&pif->cic, ipl3);
}

static void setup_channels_format(struct pif* pif)
{
    size_t i = 0;
    size_t k = 0;

    while (i < PIF_RAM_SIZE && k < PIF_CHANNELS_COUNT)
    {
        switch(pif->ram[i])
        {
        case 0x00: /* skip channel */
            disable_pif_channel(&pif->channels[k++]);
            ++i;
            break;

        case 0xff: /* dummy data */
            ++i;
            break;

        case 0xfe: /* end of channel setup - remaining channels are disabled */
            while (k < PIF_CHANNELS_COUNT) {
                disable_pif_channel(&pif->channels[k++]);
            }
            break;

        case 0xfd: /* channel reset - send reset command and discard the results */ {
            static uint8_t dummy_reset_buffer[PIF_CHANNELS_COUNT][6];

            /* setup reset command Tx=1, Rx=3, cmd=0xff */
            dummy_reset_buffer[k][0] = 0x01;
            dummy_reset_buffer[k][1] = 0x03;
            dummy_reset_buffer[k][2] = 0xff;

            setup_pif_channel(&pif->channels[k], dummy_reset_buffer[k]);
            ++k;
            ++i;
            }
            break;

        default: /* setup channel */

            /* HACK?: some games sends bogus PIF commands while accessing controller paks
             * Yoshi Story, Top Gear Rally 2, Indiana Jones, ...
             * When encountering such commands, we skip this bogus byte.
             */
            if ((i+1 < PIF_RAM_SIZE) && (pif->ram[i+1] == 0xfe)) {
                ++i;
            }
            else {
                i += setup_pif_channel(&pif->channels[k++], &pif->ram[i]);
            }
        }
    }

    /* Zilmar-Spec plugin expect a call with control_id = -1 when RAM processing is done */
    if (input.controllerCommand) {
        input.controllerCommand(-1, NULL);
    }

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "PIF setup channel");
    print_pif(pif);
#endif
}



static void process_cic_challenge(struct pif* pif)
{
    char challenge[30], response[30];
    size_t i;

    /* format the 'challenge' message into 30 nibbles for X-Scale's CIC code */
    for (i = 0; i < 15; ++i)
    {
        challenge[i*2]   = (pif->ram[0x30+i] >> 4) & 0x0f;
        challenge[i*2+1] =  pif->ram[0x30+i]       & 0x0f;
    }

    /* calculate the proper response for the given challenge (X-Scale's algorithm) */
    n64_cic_nus_6105(challenge, response, CHL_LEN - 2);
    pif->ram[0x2e] = 0;
    pif->ram[0x2f] = 0;

    /* re-format the 'response' into a byte stream */
    for (i = 0; i < 15; ++i)
    {
        pif->ram[0x30+i] = (response[i*2] << 4) + response[i*2+1];
    }

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "PIF cic challenge");
    print_pif(pif);
#endif
}

void poweron_pif(struct pif* pif)
{
    size_t i;

    memset(pif->ram, 0, PIF_RAM_SIZE);

    for(i = 0; i < PIF_CHANNELS_COUNT; ++i) {
        disable_pif_channel(&pif->channels[i]);
    }

    poweron_af_rtc(&pif->af_rtc);
    for(i = 0; i < GAME_CONTROLLERS_COUNT; ++i) {
        poweron_game_controller(&pif->controllers[i]);
    }
}

int read_pif_ram(void* opaque, uint32_t address, uint32_t* value)
{
    struct si_controller* si = (struct si_controller*)opaque;
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08" PRIX32, address);
        *value = 0;
        return -1;
    }

    memcpy(value, si->pif.ram + addr, sizeof(*value));
    *value = sl(*value);

    return 0;
}

int write_pif_ram(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct si_controller* si = (struct si_controller*)opaque;
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08" PRIX32, address);
        return -1;
    }

    masked_write((uint32_t*)(&si->pif.ram[addr]), sl(value), sl(mask));

    process_pif_ram(si);

    return 0;
}


void process_pif_ram(struct si_controller* si)
{
    struct pif* pif = &si->pif;
    uint8_t flags = pif->ram[0x3f];
    uint8_t clrmask = 0x00;
    size_t k;

    if (flags == 0) {
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "PIF process pif ram status=0x00");
        print_pif(pif);
#endif
        return;
    }

    if (flags & 0x01)
    {
        /* setup channels then clear format flag */
        setup_channels_format(pif);
        clrmask |= 0x01;
    }

    if (flags & 0x02)
    {
        /* disable channel processing when doing CIC challenge */
        for (k = 0; k < PIF_CHANNELS_COUNT; ++k) {
            disable_pif_channel(&pif->channels[k]);
        }

        /* CIC Challenge */
        process_cic_challenge(pif);
        clrmask |= 0x02;
    }

    if (flags & 0x08)
    {
        clrmask |= 0x08;
    }

    if (flags & 0xf4)
    {
        DebugMessage(M64MSG_ERROR, "error in process_pif_ram(): %" PRIX8, flags);
    }

    pif->ram[0x3f] &= ~clrmask;
}

void update_pif_ram(struct si_controller* si)
{
    size_t k;
    struct pif* pif = &si->pif;

    /* perform PIF/Channel communications */
    for (k = 0; k < PIF_CHANNELS_COUNT; ++k) {
        process_channel(&pif->channels[k]);
    }

    /* Zilmar-Spec plugin expect a call with control_id = -1 when RAM processing is done */
    if (input.readController) {
        input.readController(-1, NULL);
    }

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "PIF post read");
    print_pif(pif);
#endif
}

