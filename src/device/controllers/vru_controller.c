/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - vru_controller.c                                        *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#include "game_controller.h"
#include "vru_controller.h"

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backends/api/controller_input_backend.h"
#include "backends/api/joybus.h"
#include "plugin/plugin.h"
#include "main/rom.h"

#ifdef COMPARE_CORE
#include "api/debugger.h"
#endif

#include <stdint.h>
#include <string.h>

enum voice_status
{
    VOICE_STATUS_READY  = 0x00,
    VOICE_STATUS_START  = 0x01,
    VOICE_STATUS_CANCEL = 0x03,
    VOICE_STATUS_BUSY   = 0x05,
    VOICE_STATUS_END    = 0x07
};

static uint8_t vru_data_crc(const uint8_t* data, size_t size)
{
    size_t i;
    uint8_t crc = 0;

    for(i = 0; i <= size; ++i)
    {
        int mask;
        for (mask = 0x80; mask >= 1; mask >>= 1)
        {
            uint8_t xor_tap = (crc & 0x80) ? 0x85 : 0x00;
            crc <<= 1;
            if (i != size && (data[i] & mask)) crc |= 1;
            crc ^= xor_tap;
        }
    }
    return crc;
}

/* VRU controller */
static void vru_controller_reset(struct game_controller* cont)
{
    cont->status = 0x00;
    if (ROM_HEADER.Country_code == 0x4A /* Japan */ || ROM_HEADER.Country_code == 0x00 /* Demo */)
        cont->voice_state = VOICE_STATUS_READY;
    else
        cont->voice_state = VOICE_STATUS_START;
    cont->load_offset = 0;
    cont->voice_init = 1;
    memset(cont->word, 0, 80);
}

const struct game_controller_flavor g_vru_controller_flavor =
{
    "VRU controller",
    JDT_VRU,
    vru_controller_reset
};

static void poweron_vru_controller(void* jbd)
{
    struct game_controller* cont = (struct game_controller*)jbd;

    cont->flavor->reset(cont);
}

static void process_vru_command(void* jbd,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf)
{
    struct game_controller* cont = (struct game_controller*)jbd;
    uint32_t input_ = 0;
    uint8_t cmd = tx_buf[0];

    /* if controller can't successfully be polled, consider it to be absent */
    if (cont->icin->get_input(cont->cin, &input_) != M64ERR_SUCCESS) {
        *rx |= 0x80;
        return;
    }

    switch (cmd)
    {
    case JCMD_RESET:
        cont->flavor->reset(cont);
        /* fall through */
    case JCMD_STATUS: {
        JOYBUS_CHECK_COMMAND_FORMAT(1, 3)

        if (cont->voice_init == 2)
        {
            /* words have been loaded, we can change the state from READY to START */
            cont->voice_state = VOICE_STATUS_START;
            cont->voice_init = 1;
        }
        else if ((input_ & 0x0020) && (cont->voice_state == VOICE_STATUS_START))
        {
            /* HACK: The Z input on the VRU controller is used to indicate that someone is talking */
            /* On Densha de Go, if the player is talking for more than ~2.5 seconds, the input is ignored */
            cont->voice_state = VOICE_STATUS_BUSY;
            cont->status = 0; /* setting the status to 0 tells the game to check the voice_status */
        }
        else if (!(input_ & 0x0020) && (cont->voice_state == VOICE_STATUS_BUSY))
        {
            cont->voice_state = VOICE_STATUS_READY;
            cont->status = 0; /* setting the status to 0 tells the game to check the voice_status */
        }

        rx_buf[0] = (uint8_t)(cont->flavor->type >> 0);
        rx_buf[1] = (uint8_t)(cont->flavor->type >> 8);
        rx_buf[2] = cont->status;
    } break;

    case JCMD_CONTROLLER_READ: {
        JOYBUS_CHECK_COMMAND_FORMAT(1, 4)
#ifdef COMPARE_CORE
        CoreCompareDataSync(4, rx_buf);
#endif
    } break;

    case JCMD_VRU_READ_STATUS: {
        JOYBUS_CHECK_COMMAND_FORMAT(3, 3)
        rx_buf[0] = cont->voice_init ? cont->voice_state : 0;
        rx_buf[1] = 0;
        rx_buf[2] = vru_data_crc(&rx_buf[0], 2);
        if (cont->load_offset > 0)
        {
            uint8_t offset = 0;
            while (cont->word[offset] == 0 && offset < 40)
                ++offset;
            if (offset == 40)
            {
                DebugMessage(M64MSG_WARNING, "Empty JCMD_VRU_WRITE.");
            }
            else if (cont->word[offset] == 3)
            {
                offset += 3;
                uint16_t length = cont->word[offset];
                if (ROM_HEADER.Country_code == 0x4A /* Japan */ || ROM_HEADER.Country_code == 0x00 /* Demo */)
                {
                    offset -= 1;
                    length = 0;
                    while (cont->word[offset + length] != 0)
                    {
                        ++length;
                    }
                    input.sendVRUWord(length, &cont->word[offset], 1);
                }
                else
                {
                    ++offset;
                    input.sendVRUWord(length, &cont->word[offset], 0);
                }
            }
            else
            {
                /* Unhandled command, could be a string/word mask.
                    For a mask:
                    "Data is right-aligned and padded with zeroes to an even length, followed with command 0004. Set bits allow strings, unset ignores."
                    I haven't seen Hey You Pikachu or Densha de GO use the mask command, so I wasn't able to test.
                    TODO: Call input.SetVRUWordMask() to tell the input plugin about the mask settings */
                DebugMessage(M64MSG_WARNING, "Unknown command in JCMD_VRU_WRITE.");
            }
            cont->load_offset = 0;
        }
        cont->status = 1;
    } break;

    case JCMD_VRU_WRITE_CONFIG: {
        JOYBUS_CHECK_COMMAND_FORMAT(7, 1)
        rx_buf[0] = vru_data_crc(&tx_buf[3], 4);
        if (rx_buf[0] == 0x4E)
        {
            input.setMicState(1);
            cont->voice_init = 2;
        }
        else if (rx_buf[0] == 0xEF)
        {
            input.setMicState(0);
        }
        else if (tx_buf[3] == 0x2)
        {
            cont->voice_init = 0;
            input.clearVRUWords(tx_buf[5]);
        }
        cont->status = 0; /* status is always set to 0 after a write */
    } break;

    case JCMD_VRU_WRITE_INIT: {
        JOYBUS_CHECK_COMMAND_FORMAT(3, 1)
        if (*((uint16_t*)(&tx_buf[1])) == 0)
            input.setMicState(0);
        rx_buf[0] = 0;
    } break;

    case JCMD_VRU_READ: {
        JOYBUS_CHECK_COMMAND_FORMAT(3, 37)
        *((uint16_t*)(&rx_buf[0])) = 0x8000; /* as per zoinkity https://pastebin.com/6UiErk5h */
        *((uint16_t*)(&rx_buf[2])) = 0x0F00; /* as per zoinkity https://pastebin.com/6UiErk5h */
        *((uint16_t*)(&rx_buf[34])) = 0x0040; /* as per zoinkity https://pastebin.com/6UiErk5h */
        input.readVRUResults((uint16_t*)&rx_buf[4] /*error flags*/, (uint16_t*)&rx_buf[6] /*number of results*/, (uint16_t*)&rx_buf[8] /*mic level*/, \
            (uint16_t*)&rx_buf[10] /*voice level*/, (uint16_t*)&rx_buf[12] /*voice length*/, (uint16_t*)&rx_buf[14] /*matches*/);
        rx_buf[36] = vru_data_crc(&rx_buf[0], 36);
        cont->voice_state = VOICE_STATUS_START;
    } break;

    case JCMD_VRU_WRITE: {
        JOYBUS_CHECK_COMMAND_FORMAT(23, 1)
        rx_buf[0] = vru_data_crc(&tx_buf[3], 20);
        if (cont->load_offset == 0)
            memset(cont->word, 0, 80);
        memcpy(&cont->word[cont->load_offset], &tx_buf[3], 20);
        cont->load_offset += 10;
        cont->status = 0; /* status is always set to 0 after a write */
    } break;

    default:
        DebugMessage(M64MSG_WARNING, "cont: Unknown command %02x %02x %02x",
            *tx, *rx, cmd);
    }
}

const struct joybus_device_interface g_ijoybus_vru_controller =
{
    poweron_vru_controller,
    process_vru_command,
    NULL
};
