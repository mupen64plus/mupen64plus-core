/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - gb_cart.c                                               *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Bobby Smiles                                       *
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

/* Most of the mappers information comes from
 * "The Cycle-Accurate Game Boy Docs" by AntonioND
 */

#include "gb_cart.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include <assert.h>
#include <string.h>

/* various helper functions for ram, rom, or MBC uses */


static void read_rom(const struct storage_backend* rom, uint16_t address, uint8_t* data, size_t size)
{
    assert(size > 0);

    if (address + size > rom->size)
    {
        DebugMessage(M64MSG_WARNING, "Out of bound read from GB ROM %04x", address);
        return;
    }

    memcpy(data, &rom->data[address], size);
}


static void read_ram(const struct storage_backend* ram, unsigned int enabled, uint16_t address, uint8_t* data, size_t size)
{
    assert(size > 0);

    /* RAM has to be enabled before use */
    if (!enabled) {
        DebugMessage(M64MSG_WARNING, "Trying to read from non enabled GB RAM %04x", address);
        memset(data, 0xff, size);
        return;
    }

    /* RAM must be present */
    if (ram->data == NULL) {
        DebugMessage(M64MSG_WARNING, "Trying to read from absent GB RAM %04x", address);
        memset(data, 0xff, size);
        return;
    }

    if (address + size > ram->size)
    {
        DebugMessage(M64MSG_WARNING, "Out of bound read from GB RAM %04x", address);
        return;
    }

    memcpy(data, &ram->data[address], size);
}

static void write_ram(struct storage_backend* ram, unsigned int enabled, uint16_t address, const uint8_t* data, size_t size)
{
    assert(size > 0);

    /* RAM has to be enabled before use */
    if (!enabled) {
        DebugMessage(M64MSG_WARNING, "Trying to write to non enabled GB RAM %04x", address);
        return;
    }

    /* RAM must be present */
    if (ram->data == NULL) {
        DebugMessage(M64MSG_WARNING, "Trying to write to absent GB RAM %04x", address);
        return;
    }

    if (address + size > ram->size)
    {
        DebugMessage(M64MSG_WARNING, "Out of bound write to GB RAM %04x", address);
        return;
    }

    memcpy(&ram->data[address], data, size);
}


static void set_ram_enable(struct gb_cart* gb_cart, uint8_t value)
{
    gb_cart->ram_enable = ((value & 0x0f) == 0x0a) ? 1 : 0;
    DebugMessage(M64MSG_VERBOSE, "RAM enable = %02x", gb_cart->ram_enable);
}




static int read_gb_cart_nombc(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    switch(address >> 13)
    {
    /* 0x0000-0x7fff: ROM */
    case (0x0000 >> 13):
    case (0x2000 >> 13):
    case (0x4000 >> 13):
    case (0x6000 >> 13):
        read_rom(&gb_cart->rom, address, data, size);
        break;

    /* 0xa000-0xbfff: RAM */
    case (0xa000 >> 13):
        read_ram(&gb_cart->ram, 1, address - 0xa000, data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart read (nombc): %04x", address);
    }

    return 0;
}

static int write_gb_cart_nombc(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    switch(address >> 13)
    {
    /* 0x0000-0x7fff: ROM */
    case (0x0000 >> 13):
    case (0x2000 >> 13):
    case (0x4000 >> 13):
    case (0x6000 >> 13):
        DebugMessage(M64MSG_VERBOSE, "Trying to write to GB ROM %04x", address);
        break;

    /* 0xa000-0xbfff: RAM */
    case (0xa000 >> 13):
        write_ram(&gb_cart->ram, 1, address - 0xa000, data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart write (nombc): %04x", address);
    }

    return 0;
}


static int read_gb_cart_mbc1(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    switch(address >> 13)
    {
    /* 0x0000-0x3fff: ROM bank 00 */
    case (0x0000 >> 13):
    case (0x2000 >> 13):
        read_rom(&gb_cart->rom, address, data, size);
        break;

    /* 0x4000-0x7fff: ROM bank 01-7f */
    case (0x4000 >> 13):
    case (0x6000 >> 13):
        read_rom(&gb_cart->rom, (address - 0x4000) + (gb_cart->rom_bank * 0x4000), data, size);
        break;

    /* 0xa000-0xbfff: RAM bank 00-03 */
    case (0xa000 >> 13):
        read_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart read (MBC1): %04x", address);
    }

    return 0;
}

static int write_gb_cart_mbc1(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    uint8_t bank;
    uint8_t value = data[size-1];

    switch(address >> 13)
    {
    /* 0x0000-0x1fff: RAM enable */
    case (0x0000 >> 13):
        set_ram_enable(gb_cart, value);
        break;

    /* 0x2000-0x3fff: ROM bank select (low 5 bits) */
    case (0x2000 >> 13):
        bank = value & 0x1f;
        gb_cart->rom_bank = (gb_cart->rom_bank & ~UINT8_C(0x1f)) | (bank == 0) ? 1 : bank;
        DebugMessage(M64MSG_VERBOSE, "MBC1 set rom bank %02x", gb_cart->rom_bank);
        break;

    /* 0x4000-0x5fff: RAM bank / upper ROM bank select (2 bits) */
    case (0x4000 >> 13):
        bank = value & 0x3;
        if (gb_cart->mbc1_mode == 0) {
            /* ROM mode */
            gb_cart->rom_bank = (gb_cart->rom_bank & 0x1f) | (bank << 5);
        }
        else {
            /* RAM mode */
            gb_cart->ram_bank = bank;
        }
        DebugMessage(M64MSG_VERBOSE, "MBC1 set ram bank %02x", gb_cart->ram_bank);
        break;

    /* 0x6000-0x7fff: ROM/RAM mode (1 bit) */
    case (0x6000 >> 13):
        gb_cart->mbc1_mode = (value & 0x1);
        if (gb_cart->mbc1_mode == 0) {
            /* only RAM bank 0 is accessible in ROM mode */
            gb_cart->ram_bank = 0;
        } else {
            /* only ROM banks 0x01 - 0x1f are accessible in RAM mode */
            gb_cart->rom_bank &= 0x1f;
        }
        break;

    /* 0xa000-0xbfff: RAM bank 00-03 */
    case (0xa000 >> 13):
        write_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart write (MBC1): %04x", address);
    }

    return 0;
}

static int read_gb_cart_mbc2(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_mbc2(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}


static int read_gb_cart_mbc3(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    switch(address >> 13)
    {
    /* 0x0000-0x3fff: ROM bank 00 */
    case (0x0000 >> 13):
    case (0x2000 >> 13):
        read_rom(&gb_cart->rom, address, data, size);
        break;

    /* 0x4000-0x7fff: ROM bank 01-7f */
    case (0x4000 >> 13):
    case (0x6000 >> 13):
        read_rom(&gb_cart->rom, (address - 0x4000) + (gb_cart->rom_bank * 0x4000), data, size);
        break;

    /* 0xa000-0xbfff: RAM bank 00-07 or RTC register 08-0c */
    case (0xa000 >> 13):
        switch(gb_cart->ram_bank)
        {
        /* RAM banks */
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            read_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
            break;

        /* RTC registers */
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
            /* RAM has to be enabled before use */
            if (!gb_cart->ram_enable) {
                DebugMessage(M64MSG_WARNING, "Trying to read from non enabled GB RAM %04x", address);
                memset(data, 0xff, size);
                break;
            }

            if (!gb_cart->has_rtc) {
                DebugMessage(M64MSG_WARNING, "Trying to read from absent RTC %04x", address);
                memset(data, 0xff, size);
                break;
            }

            memset(data, read_mbc3_rtc_regs(&gb_cart->rtc, gb_cart->ram_bank - 0x08), size);
            break;

        default:
            DebugMessage(M64MSG_WARNING, "Unknown device mapped in RAM/RTC space: %04x", address);
        }
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart read (MBC3): %04x", address);
    }

    return 0;
}

static int write_gb_cart_mbc3(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    uint8_t bank;
    uint8_t value = data[size-1];

    switch(address >> 13)
    {
    /* 0x0000-0x1fff: RAM/RTC enable */
    case (0x0000 >> 13):
        set_ram_enable(gb_cart, value);
        break;

    /* 0x2000-0x3fff: ROM bank select */
    case (0x2000 >> 13):
        bank = value & 0x7f;
        gb_cart->rom_bank = (bank == 0) ? 1 : bank;
        DebugMessage(M64MSG_VERBOSE, "MBC3 set rom bank %02x", gb_cart->rom_bank);
        break;

    /* 0x4000-0x5fff: RAM bank / RTC register select */
    case (0x4000 >> 13):
        gb_cart->ram_bank = value;
        DebugMessage(M64MSG_VERBOSE, "MBC3 set ram bank %02x", gb_cart->ram_bank);
        break;

    /* 0x6000-0x7fff: latch clock registers */
    case (0x6000 >> 13):
        if (!gb_cart->has_rtc) {
            DebugMessage(M64MSG_WARNING, "Trying to latch to absent RTC %04x", address);
            break;
        }

        latch_mbc3_rtc_regs(&gb_cart->rtc, value);
        break;

    /* 0xa000-0xbfff: RAM bank 00-07 or RTC register 08-0c */
    case (0xa000 >> 13):
        switch(gb_cart->ram_bank)
        {
        /* RAM banks */
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            write_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
            break;

        /* RTC registers */
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
            /* RAM has to be enabled before use */
            if (!gb_cart->ram_enable) {
                DebugMessage(M64MSG_WARNING, "Trying to write to non enabled GB RAM %04x", address);
                break;
            }

            if (!gb_cart->has_rtc) {
                DebugMessage(M64MSG_WARNING, "Trying to write to absent RTC %04x", address);
                break;
            }

            write_mbc3_rtc_regs(&gb_cart->rtc, gb_cart->ram_bank - 0x08, value);
            break;

        default:
            DebugMessage(M64MSG_WARNING, "Unknwown device mapped in RAM/RTC space: %04x", address);
        }
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart write (MBC3): %04x", address);
    }

    return 0;
}

static int read_gb_cart_mbc5(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    switch(address >> 13)
    {
    /* 0x0000-0x3fff: ROM bank 00 */
    case (0x0000 >> 13):
    case (0x2000 >> 13):
        read_rom(&gb_cart->rom, address, data, size);
        break;

    /* 0x4000-0x7fff: ROM bank 00-ff (???) */
    case (0x4000 >> 13):
    case (0x6000 >> 13):
        read_rom(&gb_cart->rom, (address - 0x4000) + (gb_cart->rom_bank * 0x4000), data, size);
        break;

    /* 0xa000-0xbfff: RAM bank 00-07 */
    case (0xa000 >> 13):
        read_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart read (MBC5): %04x", address);
    }

    return 0;
}

static int write_gb_cart_mbc5(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    uint8_t value = data[size-1];

    switch(address >> 13)
    {
    /* 0x0000-0x1fff: RAM enable */
    case (0x0000 >> 13):
        set_ram_enable(gb_cart, value);
        break;

    /* 0x2000-0x3fff: ROM bank select */
    case (0x2000 >> 13):
        if (address < 0x3000)
        {
            gb_cart->rom_bank &= 0xff00;
            gb_cart->rom_bank |= value;
        }
        else
        {
            gb_cart->rom_bank &= 0x00ff;
            gb_cart->rom_bank |= (value & 0x01) << 8;
        }
        DebugMessage(M64MSG_VERBOSE, "MBC5 set rom bank %04x", gb_cart->rom_bank);
        break;

    /* 0x4000-0x5fff: RAM bank select */
    case (0x4000 >> 13):
        /* TODO: add rumble selection */
        gb_cart->ram_bank = value & 0x0f;
        DebugMessage(M64MSG_VERBOSE, "MBC5 set ram bank %02x", gb_cart->ram_bank);
        break;

    /* 0xa000-0xbfff: RAM bank 00-0f */
    case (0xa000 >> 13):
        /* TODO: add rumble support */
        write_ram(&gb_cart->ram, gb_cart->ram_enable, (address - 0xa000) + (gb_cart->ram_bank * 0x2000), data, size);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Invalid cart write (MBC5): %04x", address);
    }

    return 0;
}

static int read_gb_cart_mbc6(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_mbc6(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_mbc7(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_mbc7(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_mmm01(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_mmm01(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_pocket_cam(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_pocket_cam(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_bandai_tama5(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_bandai_tama5(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_huc1(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_huc1(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}

static int read_gb_cart_huc3(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return 0;
}

static int write_gb_cart_huc3(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return 0;
}



enum gbcart_extra_devices
{
    GED_NONE          = 0x00,
    GED_RAM           = 0x01,
    GED_BATTERY       = 0x02,
    GED_RTC           = 0x04,
    GED_RUMBLE        = 0x08,
    GED_ACCELEROMETER = 0x10,
};

struct parsed_cart_type
{
    const char* mbc;
    int (*read_gb_cart)(struct gb_cart*,uint16_t,uint8_t*,size_t);
    int (*write_gb_cart)(struct gb_cart*,uint16_t,const uint8_t*,size_t);
    unsigned int extra_devices;
};

static const struct parsed_cart_type* parse_cart_type(uint8_t cart_type)
{
#define MBC(x) #x, read_gb_cart_ ## x, write_gb_cart_ ## x
    static const struct parsed_cart_type nombc_none           = { MBC(nombc),        GED_NONE };
    static const struct parsed_cart_type nombc_ram            = { MBC(nombc),        GED_RAM };
    static const struct parsed_cart_type nombc_ram_batt       = { MBC(nombc),        GED_RAM | GED_BATTERY };

    static const struct parsed_cart_type mbc1_none            = { MBC(mbc1),         GED_NONE };
    static const struct parsed_cart_type mbc1_ram             = { MBC(mbc1),         GED_RAM  };
    static const struct parsed_cart_type mbc1_ram_batt        = { MBC(mbc1),         GED_RAM | GED_BATTERY };

    static const struct parsed_cart_type mbc2_none            = { MBC(mbc2),         GED_NONE };
    static const struct parsed_cart_type mbc2_ram_batt        = { MBC(mbc2),         GED_RAM | GED_BATTERY };

    static const struct parsed_cart_type mmm01_none           = { MBC(mmm01),        GED_NONE };
    static const struct parsed_cart_type mmm01_ram            = { MBC(mmm01),        GED_RAM  };
    static const struct parsed_cart_type mmm01_ram_batt       = { MBC(mmm01),        GED_RAM | GED_BATTERY };

    static const struct parsed_cart_type mbc3_none            = { MBC(mbc3),         GED_NONE };
    static const struct parsed_cart_type mbc3_ram             = { MBC(mbc3),         GED_RAM  };
    static const struct parsed_cart_type mbc3_ram_batt        = { MBC(mbc3),         GED_RAM | GED_BATTERY };
    static const struct parsed_cart_type mbc3_batt_rtc        = { MBC(mbc3),         GED_BATTERY | GED_RTC };
    static const struct parsed_cart_type mbc3_ram_batt_rtc    = { MBC(mbc3),         GED_RAM | GED_BATTERY | GED_RTC };

    static const struct parsed_cart_type mbc5_none            = { MBC(mbc5),         GED_NONE };
    static const struct parsed_cart_type mbc5_ram             = { MBC(mbc5),         GED_RAM  };
    static const struct parsed_cart_type mbc5_ram_batt        = { MBC(mbc5),         GED_RAM | GED_BATTERY };
    static const struct parsed_cart_type mbc5_rumble          = { MBC(mbc5),         GED_RUMBLE };
    static const struct parsed_cart_type mbc5_ram_rumble      = { MBC(mbc5),         GED_RAM | GED_RUMBLE };
    static const struct parsed_cart_type mbc5_ram_batt_rumble = { MBC(mbc5),         GED_RAM | GED_BATTERY | GED_RUMBLE };

    static const struct parsed_cart_type mbc6                 = { MBC(mbc6),         GED_RAM | GED_BATTERY };

    static const struct parsed_cart_type mbc7                 = { MBC(mbc7),         GED_RAM | GED_BATTERY | GED_ACCELEROMETER };

    static const struct parsed_cart_type pocket_cam           = { MBC(pocket_cam),   GED_NONE };

    static const struct parsed_cart_type bandai_tama5         = { MBC(bandai_tama5), GED_NONE };

    static const struct parsed_cart_type huc3                 = { MBC(huc3),         GED_NONE };

    static const struct parsed_cart_type huc1                 = { MBC(huc1),         GED_RAM | GED_BATTERY };
#undef MBC

    switch(cart_type)
    {
    case 0x00: return &nombc_none;
    case 0x01: return &mbc1_none;
    case 0x02: return &mbc1_ram;
    case 0x03: return &mbc1_ram_batt;
    /* 0x04 is unused */
    case 0x05: return &mbc2_none;
    case 0x06: return &mbc2_ram_batt;
    /* 0x07 is unused */
    case 0x08: return &nombc_ram;
    case 0x09: return &nombc_ram_batt;
    /* 0x0a is unused */
    case 0x0b: return &mmm01_none;
    case 0x0c: return &mmm01_ram;
    case 0x0d: return &mmm01_ram_batt;
    /* 0x0e is unused */
    case 0x0f: return &mbc3_batt_rtc;
    case 0x10: return &mbc3_ram_batt_rtc;
    case 0x11: return &mbc3_none;
    case 0x12: return &mbc3_ram;
    case 0x13: return &mbc3_ram_batt;
    /* 0x14-0x18 are unused */
    case 0x19: return &mbc5_none;
    case 0x1a: return &mbc5_ram;
    case 0x1b: return &mbc5_ram_batt;
    case 0x1c: return &mbc5_rumble;
    case 0x1d: return &mbc5_ram_rumble;
    case 0x1e: return &mbc5_ram_batt_rumble;
    /* 0x1f is unused */
    case 0x20: return &mbc6;
    /* 0x21 is unused */
    case 0x22: return &mbc7;
    /* 0x23-0xfb are unused */
    case 0xfc: return &pocket_cam;
    case 0xfd: return &bandai_tama5;
    case 0xfe: return &huc3;
    case 0xff: return &huc1;
    default:   return NULL;
    }
}


int init_gb_cart(struct gb_cart* gb_cart,
        void* rom_opaque, void (*init_rom)(void* opaque, struct storage_backend* rom),
        void* ram_opaque, void (*init_ram)(void* opaque, struct storage_backend* ram),
        struct clock_backend* clock)
{
    const struct parsed_cart_type* type;
    struct storage_backend rom;
    struct storage_backend ram;
    struct mbc3_rtc rtc;

    memset(&rom, 0, sizeof(rom));
    memset(&ram, 0, sizeof(ram));
    memset(&rtc, 0, sizeof(rtc));

    /* ask to load rom and initialize rom storage backend */
    init_rom(rom_opaque, &rom);

    /* check rom */
    if (rom.data == NULL || rom.size < 0x8000)
    {
        DebugMessage(M64MSG_ERROR, "Invalid GB ROM file size (< 32k)");
        return -1;
    }

    /* get and parse cart type */
    uint8_t cart_type = rom.data[0x147];
    type = parse_cart_type(cart_type);
    if (type == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Invalid GB cart type (%02x)", cart_type);
        return -1;
    }

    DebugMessage(M64MSG_INFO, "GB cart type (%02x) %s%s%s%s%s",
            cart_type,
            type->mbc,
            (type->extra_devices & GED_RAM)     ? " RAM" : "",
            (type->extra_devices & GED_BATTERY) ? " BATT" : "",
            (type->extra_devices & GED_RTC)     ? " RTC" : "",
            (type->extra_devices & GED_RUMBLE)  ? " RUMBLE" : "");

    /* load ram (if present) */
    if (type->extra_devices & GED_RAM)
    {
        ram.size = 0;
        switch(rom.data[0x149])
        {
        case 0x00: ram.size = (strcmp(type->mbc, "mbc2") == 0)
                            ? 0x200 /* MBC2 have an integrated 512x4bit RAM */
                            : 0;
                   break;
        case 0x01: ram.size =  1*0x800; break;
        case 0x02: ram.size =  4*0x800; break;
        case 0x03: ram.size = 16*0x800; break;
        case 0x04: ram.size = 64*0x800; break;
        case 0x05: ram.size = 32*0x800; break;
        }

        if (ram.size != 0)
        {
            size_t size = ram.size;

            init_ram(ram_opaque, &ram);
            if (ram.data == NULL || ram.size != size)
            {
                DebugMessage(M64MSG_ERROR, "Cannot get GB RAM (%d bytes)", ram.size);
                return -1;
            }
            DebugMessage(M64MSG_INFO, "Using a %d bytes GB RAM", ram.size);
        }
    }

    /* set RTC clock (if present) */
    if (type->extra_devices & GED_RTC) {
        init_mbc3_rtc(&rtc, clock);
    }

    /* update gb_cart */
    gb_cart->rom = rom;
    gb_cart->ram = ram;
    gb_cart->has_rtc = (type->extra_devices & GED_RTC) ? 1 : 0;
    gb_cart->rtc = rtc;
    gb_cart->read_gb_cart = type->read_gb_cart;
    gb_cart->write_gb_cart = type->write_gb_cart;

    return 0;
}

void poweron_gb_cart(struct gb_cart* gb_cart)
{
    gb_cart->rom_bank = 1;
    gb_cart->ram_bank = 0;
    gb_cart->ram_enable = 0;
    gb_cart->mbc1_mode = 0;

    if (gb_cart->has_rtc) {
        poweron_mbc3_rtc(&gb_cart->rtc);
    }
}

int read_gb_cart(struct gb_cart* gb_cart, uint16_t address, uint8_t* data, size_t size)
{
    return gb_cart->read_gb_cart(gb_cart, address, data, size);
}

int write_gb_cart(struct gb_cart* gb_cart, uint16_t address, const uint8_t* data, size_t size)
{
    return gb_cart->write_gb_cart(gb_cart, address, data, size);
}

