/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rom.c                                                   *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2008 Tillin9                                            *
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

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "api/config.h"
#include "api/m64p_config.h"
#include "api/m64p_types.h"
#include "device/device.h"
#include "main.h"
#include "md5.h"
#include "osal/files.h"
#include "osal/preproc.h"
#include "osd/osd.h"
#include "rom.h"
#include "util.h"

#define CHUNKSIZE 1024*128 /* Read files 128KB at a time. */

/* Number of cpu cycles per instruction */
enum { DEFAULT_COUNT_PER_OP = 2 };
/* by default, extra mem is enabled */
enum { DEFAULT_DISABLE_EXTRA_MEM = 0 };
/* Default SI DMA duration */
enum { DEFAULT_SI_DMA_DURATION = 0x900 };

static romdatabase_entry* ini_search_by_md5(md5_byte_t* md5);

static _romdatabase g_romdatabase;

/* Global loaded rom size. */
int g_rom_size = 0;

m64p_rom_header   ROM_HEADER;
rom_params        ROM_PARAMS;
m64p_rom_settings ROM_SETTINGS;

static m64p_system_type rom_country_code_to_system_type(uint16_t country_code);

static const uint8_t Z64_SIGNATURE[4] = { 0x80, 0x37, 0x12, 0x40 };
static const uint8_t V64_SIGNATURE[4] = { 0x37, 0x80, 0x40, 0x12 };
static const uint8_t N64_SIGNATURE[4] = { 0x40, 0x12, 0x37, 0x80 };

/* Tests if a file is a valid N64 rom by checking the first 4 bytes. */
static int is_valid_rom(const unsigned char *buffer)
{
    if (memcmp(buffer, Z64_SIGNATURE, sizeof(Z64_SIGNATURE)) == 0
     || memcmp(buffer, V64_SIGNATURE, sizeof(V64_SIGNATURE)) == 0
     || memcmp(buffer, N64_SIGNATURE, sizeof(N64_SIGNATURE)) == 0)
        return 1;
    else
        return 0;
}

/* Copies the source block of memory to the destination block of memory while
 * switching the endianness of .v64 and .n64 images to the .z64 format, which
 * is native to the Nintendo 64. The data extraction routines and MD5 hashing
 * function may only act on the .z64 big-endian format.
 *
 * IN: src: The source block of memory. This must be a valid Nintendo 64 ROM
 *          image of 'len' bytes.
 *     len: The length of the source and destination, in bytes.
 * OUT: dst: The destination block of memory. This must be a valid buffer for
 *           at least 'len' bytes.
 *      imagetype: A pointer to a byte that gets updated with the value of
 *                 V64IMAGE, N64IMAGE or Z64IMAGE according to the format of
 *                 the source block. The value is undefined if 'src' does not
 *                 represent a valid Nintendo 64 ROM image.
 */
static void swap_copy_rom(void* dst, const void* src, size_t len, unsigned char* imagetype)
{
    if (memcmp(src, V64_SIGNATURE, sizeof(V64_SIGNATURE)) == 0)
    {
        size_t i;
        const uint16_t* src16 = (const uint16_t*) src;
        uint16_t* dst16 = (uint16_t*) dst;

        *imagetype = V64IMAGE;
        /* .v64 images have byte-swapped half-words (16-bit). */
        for (i = 0; i < len; i += 2)
        {
            *dst16++ = m64p_swap16(*src16++);
        }
    }
    else if (memcmp(src, N64_SIGNATURE, sizeof(N64_SIGNATURE)) == 0)
    {
        size_t i;
        const uint32_t* src32 = (const uint32_t*) src;
        uint32_t* dst32 = (uint32_t*) dst;

        *imagetype = N64IMAGE;
        /* .n64 images have byte-swapped words (32-bit). */
        for (i = 0; i < len; i += 4)
        {
            *dst32++ = m64p_swap32(*src32++);
        }
    }
    else {
        *imagetype = Z64IMAGE;
        memcpy(dst, src, len);
    }
}

m64p_error open_rom(const unsigned char* romimage, unsigned int size)
{
    md5_state_t state;
    md5_byte_t digest[16];
    romdatabase_entry* entry;
    char buffer[256];
    unsigned char imagetype;
    int i;

    /* check input requirements */
    if (romimage == NULL || !is_valid_rom(romimage))
    {
        DebugMessage(M64MSG_ERROR, "open_rom(): not a valid ROM image");
        return M64ERR_INPUT_INVALID;
    }

    /* Clear Byte-swapped flag, since ROM is now deleted. */
    g_RomWordsLittleEndian = 0;
    /* allocate new buffer for ROM and copy into this buffer */
    g_rom_size = size;
    swap_copy_rom((uint8_t*)mem_base_u32(g_mem_base, MM_CART_ROM), romimage, size, &imagetype);
    /* ROM is now in N64 native (big endian) byte order */

    memcpy(&ROM_HEADER, (uint8_t*)mem_base_u32(g_mem_base, MM_CART_ROM), sizeof(m64p_rom_header));

    /* Calculate MD5 hash  */
    md5_init(&state);
    md5_append(&state, (const md5_byte_t*)((uint8_t*)mem_base_u32(g_mem_base, MM_CART_ROM)), g_rom_size);
    md5_finish(&state, digest);
    for ( i = 0; i < 16; ++i )
        sprintf(buffer+i*2, "%02X", digest[i]);
    buffer[32] = '\0';
    strcpy(ROM_SETTINGS.MD5, buffer);

    /* add some useful properties to ROM_PARAMS */
    ROM_PARAMS.systemtype = rom_country_code_to_system_type(ROM_HEADER.Country_code);
    ROM_PARAMS.cheats = NULL;

    memcpy(ROM_PARAMS.headername, ROM_HEADER.Name, 20);
    ROM_PARAMS.headername[20] = '\0';
    trim(ROM_PARAMS.headername); /* Remove trailing whitespace from ROM name. */

    /* Look up this ROM in the .ini file and fill in goodname, etc */
    if ((entry=ini_search_by_md5(digest)) != NULL ||
        (entry=ini_search_by_crc(tohl(ROM_HEADER.CRC1),tohl(ROM_HEADER.CRC2))) != NULL)
    {
        strncpy(ROM_SETTINGS.goodname, entry->goodname, 255);
        ROM_SETTINGS.goodname[255] = '\0';
        ROM_SETTINGS.savetype = entry->savetype;
        ROM_SETTINGS.status = entry->status;
        ROM_SETTINGS.players = entry->players;
        ROM_SETTINGS.rumble = entry->rumble;
        ROM_SETTINGS.transferpak = entry->transferpak;
        ROM_SETTINGS.mempak = entry->mempak;
        ROM_SETTINGS.biopak = entry->biopak;
        ROM_SETTINGS.countperop = entry->countperop;
        ROM_SETTINGS.disableextramem = entry->disableextramem;
        ROM_SETTINGS.sidmaduration = entry->sidmaduration;
        ROM_PARAMS.cheats = entry->cheats;
    }
    else
    {
        strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
        strcat(ROM_SETTINGS.goodname, " (unknown rom)");
        /* There's no way to guess the save type, but 4K EEPROM is better than nothing */
        ROM_SETTINGS.savetype = SAVETYPE_EEPROM_4K;
        ROM_SETTINGS.status = 0;
        ROM_SETTINGS.players = 4;
        ROM_SETTINGS.rumble = 1;
        ROM_SETTINGS.transferpak = 0;
        ROM_SETTINGS.mempak = 1;
        ROM_SETTINGS.biopak = 0;
        ROM_SETTINGS.countperop = DEFAULT_COUNT_PER_OP;
        ROM_SETTINGS.disableextramem = DEFAULT_DISABLE_EXTRA_MEM;
        ROM_SETTINGS.sidmaduration = DEFAULT_SI_DMA_DURATION;
        ROM_PARAMS.cheats = NULL;
    }

    /* print out a bunch of info about the ROM */
    DebugMessage(M64MSG_INFO, "Goodname: %s", ROM_SETTINGS.goodname);
    DebugMessage(M64MSG_INFO, "Name: %s", ROM_HEADER.Name);
    imagestring(imagetype, buffer);
    DebugMessage(M64MSG_INFO, "MD5: %s", ROM_SETTINGS.MD5);
    DebugMessage(M64MSG_INFO, "CRC: %08" PRIX32 " %08" PRIX32, tohl(ROM_HEADER.CRC1), tohl(ROM_HEADER.CRC2));
    DebugMessage(M64MSG_INFO, "Imagetype: %s", buffer);
    DebugMessage(M64MSG_INFO, "Rom size: %d bytes (or %d Mb or %d Megabits)", g_rom_size, g_rom_size/1024/1024, g_rom_size/1024/1024*8);
    DebugMessage(M64MSG_VERBOSE, "ClockRate = %" PRIX32, tohl(ROM_HEADER.ClockRate));
    DebugMessage(M64MSG_INFO, "Version: %" PRIX32, tohl(ROM_HEADER.Release));
    if(tohl(ROM_HEADER.Manufacturer_ID) == 'N')
        DebugMessage(M64MSG_INFO, "Manufacturer: Nintendo");
    else
        DebugMessage(M64MSG_INFO, "Manufacturer: %" PRIX32, tohl(ROM_HEADER.Manufacturer_ID));
    DebugMessage(M64MSG_VERBOSE, "Cartridge_ID: %" PRIX16, ROM_HEADER.Cartridge_ID);
    countrycodestring(ROM_HEADER.Country_code, buffer);
    DebugMessage(M64MSG_INFO, "Country: %s", buffer);
    DebugMessage(M64MSG_VERBOSE, "PC = %" PRIX32, tohl(ROM_HEADER.PC));
    DebugMessage(M64MSG_VERBOSE, "Save type: %d", ROM_SETTINGS.savetype);

    return M64ERR_SUCCESS;
}

m64p_error close_rom(void)
{
    /* Clear Byte-swapped flag, since ROM is now deleted. */
    g_RomWordsLittleEndian = 0;
    DebugMessage(M64MSG_STATUS, "Rom closed.");

    return M64ERR_SUCCESS;
}

/********************************************************************************************/
/* ROM utility functions */

// Get the system type associated to a ROM country code.
static m64p_system_type rom_country_code_to_system_type(uint16_t country_code)
{
    switch (country_code & UINT16_C(0xFF))
    {
        // PAL codes
        case 0x44:
        case 0x46:
        case 0x49:
        case 0x50:
        case 0x53:
        case 0x55:
        case 0x58:
        case 0x59:
            return SYSTEM_PAL;

        // NTSC codes
        case 0x37:
        case 0x41:
        case 0x45:
        case 0x4a:
        default: // Fallback for unknown codes
            return SYSTEM_NTSC;
    }
}

static size_t romdatabase_resolve_round(void)
{
    romdatabase_search *entry;
    romdatabase_entry *ref;
    size_t skipped = 0;

    /* Resolve RefMD5 references */
    for (entry = g_romdatabase.list; entry; entry = entry->next_entry) {
        if (!entry->entry.refmd5)
            continue;

        ref = ini_search_by_md5(entry->entry.refmd5);
        if (!ref) {
            DebugMessage(M64MSG_WARNING, "ROM Database: Error solving RefMD5s");
            continue;
        }

        /* entry is not yet resolved, skip for now */
        if (ref->refmd5) {
            skipped++;
            continue;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_GOODNAME) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_GOODNAME)) {
            entry->entry.goodname = strdup(ref->goodname);
            if (entry->entry.goodname)
                entry->entry.set_flags |= ROMDATABASE_ENTRY_GOODNAME;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_CRC) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_CRC)) {
            entry->entry.crc1 = ref->crc1;
            entry->entry.crc2 = ref->crc2;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_CRC;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_STATUS) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_STATUS)) {
            entry->entry.status = ref->status;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_STATUS;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_SAVETYPE) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_SAVETYPE)) {
            entry->entry.savetype = ref->savetype;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_PLAYERS) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_PLAYERS)) {
            entry->entry.players = ref->players;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_PLAYERS;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_RUMBLE) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_RUMBLE)) {
            entry->entry.rumble = ref->rumble;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_RUMBLE;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_COUNTEROP) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_COUNTEROP)) {
            entry->entry.countperop = ref->countperop;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_COUNTEROP;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_CHEATS) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_CHEATS)) {
            if (ref->cheats)
                entry->entry.cheats = strdup(ref->cheats);
            entry->entry.set_flags |= ROMDATABASE_ENTRY_CHEATS;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_EXTRAMEM) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_EXTRAMEM)) {
            entry->entry.disableextramem = ref->disableextramem;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_EXTRAMEM;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_TRANSFERPAK) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_TRANSFERPAK)) {
            entry->entry.transferpak = ref->transferpak;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_TRANSFERPAK;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_MEMPAK) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_MEMPAK)) {
            entry->entry.mempak = ref->mempak;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_MEMPAK;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_BIOPAK) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_BIOPAK)) {
            entry->entry.biopak = ref->biopak;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_BIOPAK;
        }

        if (!isset_bitmask(entry->entry.set_flags, ROMDATABASE_ENTRY_SIDMADURATION) &&
            isset_bitmask(ref->set_flags, ROMDATABASE_ENTRY_SIDMADURATION)) {
            entry->entry.sidmaduration = ref->sidmaduration;
            entry->entry.set_flags |= ROMDATABASE_ENTRY_SIDMADURATION;
        }

        free(entry->entry.refmd5);
        entry->entry.refmd5 = NULL;
    }

    return skipped;
}

static void romdatabase_resolve(void)
{
    size_t last_skipped = (size_t)~0ULL;
    size_t skipped;

    do {
        skipped = romdatabase_resolve_round();
        if (skipped == last_skipped) {
            DebugMessage(M64MSG_ERROR, "Unable to resolve rom database entries (loop)");
            break;
        }
        last_skipped = skipped;
    } while (skipped > 0);
}

/********************************************************************************************/
/* INI Rom database functions */

void romdatabase_open(void)
{
    FILE *fPtr;
    char buffer[256];
    romdatabase_search* search = NULL;
    romdatabase_search** next_search;

    int counter, value, lineno;
    unsigned char index;
    const char *pathname = ConfigGetSharedDataFilepath("mupen64plus.ini");

    if(g_romdatabase.have_database)
        return;

    /* Open romdatabase. */
    if (pathname == NULL || (fPtr = osal_file_open(pathname, "rb")) == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Unable to open rom database file '%s'.", pathname);
        return;
    }

    g_romdatabase.have_database = 1;

    /* Clear premade indices. */
    for(counter = 0; counter < 255; ++counter)
        g_romdatabase.crc_lists[counter] = NULL;
    for(counter = 0; counter < 255; ++counter)
        g_romdatabase.md5_lists[counter] = NULL;
    g_romdatabase.list = NULL;

    next_search = &g_romdatabase.list;

    /* Parse ROM database file */
    for (lineno = 1; fgets(buffer, 255, fPtr) != NULL; lineno++)
    {
        char *line = buffer;
        ini_line l = ini_parse_line(&line);
        switch (l.type)
        {
        case INI_SECTION:
        {
            md5_byte_t md5[16];
            if (!parse_hex(l.name, md5, 16))
            {
                DebugMessage(M64MSG_WARNING, "ROM Database: Invalid MD5 on line %i", lineno);
                search = NULL;
                continue;
            }

            *next_search = (romdatabase_search*) malloc(sizeof(romdatabase_search));
            search = *next_search;
            next_search = &search->next_entry;

            memset(search, 0, sizeof(romdatabase_search));

            search->entry.goodname = NULL;
            memcpy(search->entry.md5, md5, 16);
            search->entry.refmd5 = NULL;
            search->entry.crc1 = 0;
            search->entry.crc2 = 0;
            search->entry.status = 0; /* Set default to 0 stars. */
            search->entry.savetype = SAVETYPE_EEPROM_4K;
            search->entry.players = 4;
            search->entry.rumble = 1;
            search->entry.countperop = DEFAULT_COUNT_PER_OP;
            search->entry.disableextramem = DEFAULT_DISABLE_EXTRA_MEM;
            search->entry.cheats = NULL;
            search->entry.transferpak = 0;
            search->entry.mempak = 1;
            search->entry.biopak = 0;
            search->entry.sidmaduration = DEFAULT_SI_DMA_DURATION;
            search->entry.set_flags = ROMDATABASE_ENTRY_NONE;

            search->next_entry = NULL;
            search->next_crc = NULL;
            /* Index MD5s by first 8 bits. */
            index = search->entry.md5[0];
            search->next_md5 = g_romdatabase.md5_lists[index];
            g_romdatabase.md5_lists[index] = search;

            break;
        }
        case INI_PROPERTY:
            // This happens if there's stray properties before any section,
            // or if some error happened on INI_SECTION (e.g. parsing).
            if (search == NULL)
            {
                DebugMessage(M64MSG_WARNING, "ROM Database: Ignoring property on line %i", lineno);
                continue;
            }
            if(!strcmp(l.name, "GoodName"))
            {
                search->entry.goodname = strdup(l.value);
                search->entry.set_flags |= ROMDATABASE_ENTRY_GOODNAME;
            }
            else if(!strcmp(l.name, "CRC"))
            {
                char garbage_sweeper;
                if (sscanf(l.value, "%X %X%c", &search->entry.crc1,
                    &search->entry.crc2, &garbage_sweeper) == 2)
                {
                    /* Index CRCs by first 8 bits. */
                    index = search->entry.crc1 >> 24;
                    search->next_crc = g_romdatabase.crc_lists[index];
                    g_romdatabase.crc_lists[index] = search;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_CRC;
                }
                else
                {
                    search->entry.crc1 = search->entry.crc2 = 0;
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid CRC on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "RefMD5"))
            {
                md5_byte_t md5[16];
                if (parse_hex(l.value, md5, 16))
                {
                    search->entry.refmd5 = (md5_byte_t*)malloc(16*sizeof(md5_byte_t));
                    memcpy(search->entry.refmd5, md5, 16);
                }
                else
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid RefMD5 on line %i", lineno);
            }
            else if(!strcmp(l.name, "SaveType"))
            {
                if(!strcmp(l.value, "Eeprom 4KB")) {
                    search->entry.savetype = SAVETYPE_EEPROM_4K;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else if(!strcmp(l.value, "Eeprom 16KB")) {
                    search->entry.savetype = SAVETYPE_EEPROM_16K;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else if(!strcmp(l.value, "SRAM")) {
                    search->entry.savetype = SAVETYPE_SRAM;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else if(!strcmp(l.value, "Flash RAM")) {
                    search->entry.savetype = SAVETYPE_FLASH_RAM;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else if(!strcmp(l.value, "Controller Pack")) {
                    search->entry.savetype = SAVETYPE_CONTROLLER_PAK;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else if(!strcmp(l.value, "None")) {
                    search->entry.savetype = SAVETYPE_NONE;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SAVETYPE;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid save type on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "Status"))
            {
                if (string_to_int(l.value, &value) && value >= 0 && value < 6) {
                    search->entry.status = value;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_STATUS;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid status on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "Players"))
            {
                if (string_to_int(l.value, &value) && value >= 0 && value < 8) {
                    search->entry.players = value;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_PLAYERS;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid player count on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "Rumble"))
            {
                if(!strcmp(l.value, "Yes")) {
                    search->entry.rumble = 1;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_RUMBLE;
                } else if(!strcmp(l.value, "No")) {
                    search->entry.rumble = 0;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_RUMBLE;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid rumble string on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "CountPerOp"))
            {
                if (string_to_int(l.value, &value) && value > 0 && value <= 4) {
                    search->entry.countperop = value;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_COUNTEROP;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid CountPerOp on line %i", lineno);
                }
            }
            else if (!strcmp(l.name, "DisableExtraMem"))
            {
                search->entry.disableextramem = atoi(l.value);
                search->entry.set_flags |= ROMDATABASE_ENTRY_EXTRAMEM;
            }
            else if(!strncmp(l.name, "Cheat", 5))
            {
                size_t len1 = 0, len2 = 0;
                char *newcheat;

                if (search->entry.cheats)
                    len1 = strlen(search->entry.cheats);
                if (l.value)
                    len2 = strlen(l.value);

                /* initial cheat */
                if (len1 == 0 && len2 > 0)
                    search->entry.cheats = strdup(l.value);

                /* append cheat */
                if (len1 != 0 && len2 > 0) {
                    newcheat = malloc(len1 + 1 + len2 + 1);
                    if (!newcheat) {
                        DebugMessage(M64MSG_WARNING, "ROM Database: Failed to append cheat");
                    } else {
                        strcpy(newcheat, search->entry.cheats);
                        strcat(newcheat, ";");
                        strcat(newcheat, l.value);
                        free(search->entry.cheats);
                        search->entry.cheats = newcheat;
                    }
                }

                search->entry.set_flags |= ROMDATABASE_ENTRY_CHEATS;
            }
            else if(!strcmp(l.name, "Transferpak"))
            {
                if(!strcmp(l.value, "Yes")) {
                    search->entry.transferpak = 1;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_TRANSFERPAK;
                } else if(!strcmp(l.value, "No")) {
                    search->entry.transferpak = 0;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_TRANSFERPAK;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid transferpak string on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "Mempak"))
            {
                if(!strcmp(l.value, "Yes")) {
                    search->entry.mempak = 1;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_MEMPAK;
                } else if(!strcmp(l.value, "No")) {
                    search->entry.mempak = 0;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_MEMPAK;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid mempak string on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "Biopak"))
            {
                if(!strcmp(l.value, "Yes")) {
                    search->entry.biopak = 1;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_BIOPAK;
                } else if(!strcmp(l.value, "No")) {
                    search->entry.biopak = 0;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_BIOPAK;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid biopak string on line %i", lineno);
                }
            }
            else if(!strcmp(l.name, "SiDmaDuration"))
            {
                if (string_to_int(l.value, &value) && value >= 0 && value <= 0x10000) {
                    search->entry.sidmaduration = value;
                    search->entry.set_flags |= ROMDATABASE_ENTRY_SIDMADURATION;
                } else {
                    DebugMessage(M64MSG_WARNING, "ROM Database: Invalid SiDmaDuration on line %i", lineno);
                }
            }
            else
            {
                DebugMessage(M64MSG_WARNING, "ROM Database: Unknown property on line %i", lineno);
            }
            break;
        default:
            break;
        }
    }

    fclose(fPtr);
    romdatabase_resolve();
}

void romdatabase_close(void)
{
    if (!g_romdatabase.have_database)
        return;

    while (g_romdatabase.list != NULL)
        {
        romdatabase_search* search = g_romdatabase.list->next_entry;
        if(g_romdatabase.list->entry.goodname)
            free(g_romdatabase.list->entry.goodname);
        if(g_romdatabase.list->entry.refmd5)
            free(g_romdatabase.list->entry.refmd5);
        free(g_romdatabase.list->entry.cheats);
        free(g_romdatabase.list);
        g_romdatabase.list = search;
        }
}

static romdatabase_entry* ini_search_by_md5(md5_byte_t* md5)
{
    romdatabase_search* search;

    if(!g_romdatabase.have_database)
        return NULL;

    search = g_romdatabase.md5_lists[md5[0]];

    while (search != NULL && memcmp(search->entry.md5, md5, 16) != 0)
        search = search->next_md5;

    if(search==NULL)
        return NULL;

    return &(search->entry);
}

romdatabase_entry* ini_search_by_crc(unsigned int crc1, unsigned int crc2)
{
    romdatabase_search* search;
    romdatabase_entry* found_entry = NULL;

    if(!g_romdatabase.have_database) 
        return NULL;

    search = g_romdatabase.crc_lists[((crc1 >> 24) & 0xff)];

    // because CRCs can be ambiguous (there can be multiple database entries with the same CRC),
    // we will prefer MD5 hashes instead. If the given CRC matches more than one entry in the
    // database, we will return no match.
    while (search != NULL)
    {
        if (search->entry.crc1 == crc1 && search->entry.crc2 == crc2)
        {
            if (found_entry != NULL)
                return NULL;
            found_entry = &search->entry;
        }
        search = search->next_crc;
    }

    return found_entry;
}


