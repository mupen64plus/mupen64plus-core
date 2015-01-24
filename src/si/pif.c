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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pif.h"
#include "n64_cic_nus_6105.h"
#include "si_controller.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/debugger.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/r4300.h"
#include "r4300/interupt.h"

static unsigned char mempack[4][0x8000];

static char *get_mempack_path(void)
{
    return formatstr("%s%s.mpk", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void mempack_format(void)
{
    unsigned char init[] =
    {
        0x81,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x71,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03
    };
    int i,j;
    for (i=0; i<4; i++)
    {
        for (j=0; j<0x8000; j+=2)
        {
            mempack[i][j] = 0;
            mempack[i][j+1] = 0x03;
        }
        memcpy(mempack[i], init, 272);
    }
}

static void mempack_read_file(void)
{
    char *filename = get_mempack_path();

    switch (read_from_file(filename, mempack, sizeof(mempack)))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open memory pack file '%s' for reading", filename);
            mempack_format();
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed for 128kb mempack file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void mempack_write_file(void)
{
    char *filename = get_mempack_path();

    switch (write_to_file(filename, mempack, sizeof(mempack)))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open memory pack file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed for 128kb mempack file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

//#define DEBUG_PIF
#ifdef DEBUG_PIF
void print_pif(struct pif* pif)
{
    int i;
    for (i=0; i<(64/8); i++)
        DebugMessage(M64MSG_INFO, "%x %x %x %x | %x %x %x %x",
                     pif->ram[i*8+0], pif->ram[i*8+1],pif->ram[i*8+2], pif->ram[i*8+3],
                     pif->ram[i*8+4], pif->ram[i*8+5],pif->ram[i*8+6], pif->ram[i*8+7]);
}
#endif

static unsigned char mempack_crc(unsigned char *data)
{
    int i;
    unsigned char CRC = 0;
    for (i=0; i<=0x20; i++)
    {
        int mask;
        for (mask = 0x80; mask >= 1; mask >>= 1)
        {
            int xor_tap = (CRC & 0x80) ? 0x85 : 0x00;
            CRC <<= 1;
            if (i != 0x20 && (data[i] & mask)) CRC |= 1;
            CRC ^= xor_tap;
        }
    }
    return CRC;
}

static void internal_ReadController(int Control, unsigned char *Command)
{
    switch (Command[2])
    {
    case 1:
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "internal_ReadController() Channel %i Command 1 read buttons", Control);
#endif
        if (Controls[Control].Present)
        {
            BUTTONS Keys;
            input.getKeys(Control, &Keys);
            *((unsigned int *)(Command + 3)) = Keys.Value;
#ifdef COMPARE_CORE
            CoreCompareDataSync(4, Command+3);
#endif
        }
        break;
    case 2: // read controller pack
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "internal_ReadController() Channel %i Command 2 read controller pack (in Input plugin)", Control);
#endif
        if (Controls[Control].Present)
        {
            if (Controls[Control].Plugin == PLUGIN_RAW)
                if (input.readController)
                    input.readController(Control, Command);
        }
        break;
    case 3: // write controller pack
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "internal_ReadController() Channel %i Command 3 write controller pack (in Input plugin)", Control);
#endif
        if (Controls[Control].Present)
        {
            if (Controls[Control].Plugin == PLUGIN_RAW)
                if (input.readController)
                    input.readController(Control, Command);
        }
        break;
    }
}

static void internal_ControllerCommand(int Control, unsigned char *Command)
{
    switch (Command[2])
    {
    case 0x00: // read status
    case 0xFF: // reset
        if ((Command[1] & 0x80))
            break;
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command %02x check pack present", Control, Command[2]);
#endif
        if (Controls[Control].Present)
        {
            Command[3] = 0x05;
            Command[4] = 0x00;
            switch (Controls[Control].Plugin)
            {
            case PLUGIN_MEMPAK:
                Command[5] = 1;
                break;
            case PLUGIN_RAW:
                Command[5] = 1;
                break;
            default:
                Command[5] = 0;
                break;
            }
        }
        else
            Command[1] |= 0x80;
        break;
    case 0x01:
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 1 check controller present", Control);
#endif
        if (!Controls[Control].Present)
            Command[1] |= 0x80;
        break;
    case 0x02: // read controller pack
        if (Controls[Control].Present)
        {
            switch (Controls[Control].Plugin)
            {
            case PLUGIN_MEMPAK:
            {
                int address = (Command[3] << 8) | Command[4];
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 2 read mempack address %04x", Control, address);
#endif
                if (address == 0x8001)
                {
                    memset(&Command[5], 0, 0x20);
                    Command[0x25] = mempack_crc(&Command[5]);
                }
                else
                {
                    address &= 0xFFE0;
                    if (address <= 0x7FE0)
                    {
                        mempack_read_file();
                        memcpy(&Command[5], &mempack[Control][address], 0x20);
                    }
                    else
                    {
                        memset(&Command[5], 0, 0x20);
                    }
                    Command[0x25] = mempack_crc(&Command[5]);
                }
            }
            break;
            case PLUGIN_RAW:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 2 controllerCommand (in Input plugin)", Control);
#endif
                if (input.controllerCommand)
                    input.controllerCommand(Control, Command);
                break;
            default:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 2 (no pack plugged in)", Control);
#endif
                memset(&Command[5], 0, 0x20);
                Command[0x25] = 0;
            }
        }
        else
            Command[1] |= 0x80;
        break;
    case 0x03: // write controller pack
        if (Controls[Control].Present)
        {
            switch (Controls[Control].Plugin)
            {
            case PLUGIN_MEMPAK:
            {
                int address = (Command[3] << 8) | Command[4];
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 3 write mempack address %04x", Control, address);
#endif
                if (address == 0x8001)
                    Command[0x25] = mempack_crc(&Command[5]);
                else
                {
                    address &= 0xFFE0;
                    if (address <= 0x7FE0)
                    {
                        mempack_read_file();
                        memcpy(&mempack[Control][address], &Command[5], 0x20);
                        mempack_write_file();
                    }
                    Command[0x25] = mempack_crc(&Command[5]);
                }
            }
            break;
            case PLUGIN_RAW:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 3 controllerCommand (in Input plugin)", Control);
#endif
                if (input.controllerCommand)
                    input.controllerCommand(Control, Command);
                break;
            default:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO, "internal_ControllerCommand() Channel %i Command 3 (no pack plugged in)", Control);
#endif
                Command[0x25] = mempack_crc(&Command[5]);
            }
        }
        else
            Command[1] |= 0x80;
        break;
    }
}

static void process_cart_command(struct pif* pif, int channel, uint8_t* cmd)
{
    switch (cmd[2])
    {
    case 0: eeprom_status_command(pif, channel, cmd); break;
    case 4: eeprom_read_command(pif, channel, cmd); break;
    case 5: eeprom_write_command(pif, channel, cmd); break;
    case 6: af_rtc_status_command(pif, channel, cmd); break;
    case 7: af_rtc_read_command(pif, channel, cmd); break;
    case 8: af_rtc_write_command(pif, channel, cmd); break;
    default:
        DebugMessage(M64MSG_ERROR, "unknown PIF command: %02x", cmd[2]);
    }
}

void init_pif(struct pif* pif)
{
    memset(pif->ram, 0, PIF_RAM_SIZE);
}

int read_pif_ram(void* opaque, uint32_t address, uint32_t* value)
{
    struct si_controller* si = (struct si_controller*)opaque;
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08x", address);
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
        DebugMessage(M64MSG_ERROR, "Invalid PIF address: %08x", address);
        return -1;
    }

    masked_write((uint32_t*)(&si->pif.ram[addr]), sl(value), sl(mask));

    if ((addr == 0x3c) && (mask & 0xff))
    {
        if (si->pif.ram[0x3f] == 0x08)
        {
            si->pif.ram[0x3f] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
        {
            update_pif_write(si);
        }
    }

    return 0;
}


void update_pif_write(struct si_controller* si)
{
    struct pif* pif = &si->pif;

    char challenge[30], response[30];
    int i=0, channel=0;
    if (pif->ram[0x3F] > 1)
    {
        switch (pif->ram[0x3F])
        {
        case 0x02:
#ifdef DEBUG_PIF
            DebugMessage(M64MSG_INFO, "update_pif_write() pif_ram[0x3f] = 2 - CIC challenge");
#endif
            // format the 'challenge' message into 30 nibbles for X-Scale's CIC code
            for (i = 0; i < 15; i++)
            {
                challenge[i*2] =   (pif->ram[48+i] >> 4) & 0x0f;
                challenge[i*2+1] =  pif->ram[48+i]       & 0x0f;
            }
            // calculate the proper response for the given challenge (X-Scale's algorithm)
            n64_cic_nus_6105(challenge, response, CHL_LEN - 2);
            pif->ram[46] = 0;
            pif->ram[47] = 0;
            // re-format the 'response' into a byte stream
            for (i = 0; i < 15; i++)
            {
                pif->ram[48+i] = (response[i*2] << 4) + response[i*2+1];
            }
            // the last byte (2 nibbles) is always 0
            pif->ram[63] = 0;
            break;
        case 0x08:
#ifdef DEBUG_PIF
            DebugMessage(M64MSG_INFO, "update_pif_write() pif_ram[0x3f] = 8");
#endif
            pif->ram[0x3F] = 0;
            break;
        default:
            DebugMessage(M64MSG_ERROR, "error in update_pif_write(): %x", pif->ram[0x3F]);
        }
        return;
    }
    while (i<0x40)
    {
        switch (pif->ram[i])
        {
        case 0x00:
            channel++;
            if (channel > 6) i=0x40;
            break;
        case 0xFF:
            break;
        default:
            if (!(pif->ram[i] & 0xC0))
            {
                if (channel < 4)
                {
                    if (Controls[channel].Present &&
                            Controls[channel].RawData)
                        input.controllerCommand(channel, &pif->ram[i]);
                    else
                        internal_ControllerCommand(channel, &pif->ram[i]);
                }
                else if (channel == 4)
                    process_cart_command(pif, channel, &pif->ram[i]);
                else
                    DebugMessage(M64MSG_ERROR, "channel >= 4 in update_pif_write");
                i += pif->ram[i] + (pif->ram[(i+1)] & 0x3F) + 1;
                channel++;
            }
            else
                i=0x40;
        }
        i++;
    }
    //pif->ram[0x3F] = 0;
    input.controllerCommand(-1, NULL);
}

void update_pif_read(struct si_controller* si)
{
    struct pif* pif = &si->pif;

    int i=0, channel=0;
    while (i<0x40)
    {
        switch (pif->ram[i])
        {
        case 0x00:
            channel++;
            if (channel > 6) i=0x40;
            break;
        case 0xFE:
            i = 0x40;
            break;
        case 0xFF:
            break;
        case 0xB4:
        case 0x56:
        case 0xB8:
            break;
        default:
            if (!(pif->ram[i] & 0xC0))
            {
                if (channel < 4)
                {
                    if (Controls[channel].Present &&
                            Controls[channel].RawData)
                        input.readController(channel, &pif->ram[i]);
                    else
                        internal_ReadController(channel, &pif->ram[i]);
                }
                i += pif->ram[i] + (pif->ram[(i+1)] & 0x3F) + 1;
                channel++;
            }
            else
                i=0x40;
        }
        i++;
    }
    input.readController(-1, NULL);
}

