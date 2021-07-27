/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*   Mupen64plus - disk.c                                                  *
*   Mupen64Plus homepage: https://mupen64plus.org/                        *
*   Copyright (C) 2020 LuigiBlood                                         *
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

#include <stdlib.h>
#include "disk.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "backends/api/storage_backend.h"
#include "main/util.h"

static uint8_t* storage_disk_data(const void* storage)
{
    const struct dd_disk* disk = (struct dd_disk*)storage;
    return disk->istorage->data(disk->storage);
}

static size_t storage_disk_size(const void* storage)
{
    const struct dd_disk* disk = (struct dd_disk*)storage;
    return disk->istorage->size(disk->storage);
}


static void storage_disk_save_dummy(void* storage, size_t start, size_t size)
{
    /* do nothing */
}

static void storage_disk_save_full(void* storage, size_t start, size_t size)
{
    struct dd_disk* disk = (struct dd_disk*)storage;
    disk->isave_storage->save(disk->save_storage, start, size);
}

static void storage_disk_save_ram_only(void* storage, size_t start, size_t size)
{
    struct dd_disk* disk = (struct dd_disk*)storage;

    /* check range and translate start address before calling save */
    if (start >= disk->offset_ram) {
        start -= disk->offset_ram;
        if ((start + size) <= disk->isave_storage->size(disk->save_storage)) {
            disk->isave_storage->save(disk->save_storage, start, size);
        }
    }
}


const struct storage_backend_interface g_istorage_disk_read_only =
{
    storage_disk_data,
    storage_disk_size,
    storage_disk_save_dummy
};

const struct storage_backend_interface g_istorage_disk_full =
{
    storage_disk_data,
    storage_disk_size,
    storage_disk_save_full
};

const struct storage_backend_interface g_istorage_disk_ram_only =
{
    storage_disk_data,
    storage_disk_size,
    storage_disk_save_ram_only
};


/* Disk Helper routines */
void GenerateLBAToPhysTable(struct dd_disk* disk)
{
    //For SDK and D64 formats
    if (disk->format == DISK_FORMAT_MAME)
        return;

    const struct dd_sys_data* sys_data = (void*)(disk->istorage->data(disk->storage) + disk->offset_sys);
    for (uint32_t lba = 0; lba < SIZE_LBA; lba++)
    {
        disk->lba_phys_table[lba] = LBAToPhys(sys_data, lba);
    }
}

uint32_t LBAToVZone(const struct dd_sys_data* sys_data, uint32_t lba)
{
    return LBAToVZoneA(sys_data->type, lba);
}

uint32_t LBAToVZoneA(uint8_t type, uint32_t lba)
{
    for (uint32_t vzone = 0; vzone < 16; vzone++) {
        if (lba < VZoneLBATable[type & 0x0F][vzone]) {
            return vzone;
        }
    }
    return -1;
}

uint32_t LBAToByte(const struct dd_sys_data* sys_data, uint32_t lba, uint32_t nlbas)
{
    return LBAToByteA(sys_data->type, lba, nlbas);
}

uint32_t LBAToByteA(uint8_t type, uint32_t lba, uint32_t nlbas)
{
    uint8_t init_flag = 1;
    uint32_t totalbytes = 0;
    uint32_t blocksize = 0;
    uint32_t vzone = 0;
    uint32_t pzone = 0;

    uint8_t disktype = type & 0x0F;

    if (nlbas != 0)
    {
        for (; nlbas != 0; nlbas--)
        {
            if ((init_flag == 1) || (VZoneLBATable[disktype][vzone] == lba))
            {
                vzone = LBAToVZoneA(type, lba);
                pzone = VZoneToPZone(vzone, disktype);
                if (7 < pzone)
                {
                    pzone -= 7;
                }
                blocksize = zone_sec_size_phys[pzone] * SECTORS_PER_BLOCK;
            }

            totalbytes += blocksize;
            lba++;
            init_flag = 0;
            if (((nlbas - 1) != 0) && (lba > MAX_LBA))
            {
                return 0xFFFFFFFF;
            }
        }
    }

    return totalbytes;
}

uint16_t LBAToPhys(const struct dd_sys_data* sys_data, uint32_t lba)
{
    uint8_t disktype = sys_data->type & 0x0F;

    const uint16_t OUTERCYL_TBL[8] = { 0x000, 0x09E, 0x13C, 0x1D1, 0x266, 0x2FB, 0x390, 0x425 };

    //Get Block 0/1 on Disk Track
    uint8_t block = 1;
    if (((lba & 3) == 0) || ((lba & 3) == 3))
        block = 0;

    //Get Virtual & Physical Disk Zones
    uint16_t vzone = LBAToVZone(sys_data, lba);
    uint16_t pzone = VZoneToPZone(vzone, disktype);

    //Get Disk Head
    uint16_t head = (7 < pzone);

    //Get Disk Zone
    uint16_t disk_zone = pzone;
    if (disk_zone != 0)
        disk_zone = pzone - 7;

    //Get Virtual Zone LBA start, if Zone 0, it's LBA 0
    uint16_t vzone_lba = 0;
    if (vzone != 0)
        vzone_lba = VZoneLBATable[disktype][vzone - 1];

    //Calculate Physical Track
    uint16_t track = (lba - vzone_lba) >> 1;

    //Get the start track from current zone
    uint16_t track_zone_start = TrackZoneTable[0][pzone];
    if (head != 0)
    {
        //If Head 1, count from the other way around
        track = -track;
        track_zone_start = OUTERCYL_TBL[disk_zone - 1];
    }
    track += TrackZoneTable[0][pzone];

    //Get the relative offset to defect tracks for the current zone (if Zone 0, then it's 0)
    uint16_t defect_offset = 0;
    if (pzone != 0)
        defect_offset = sys_data->defect_offset[pzone - 1];

    //Get amount of defect tracks for the current zone
    uint16_t defect_amount = sys_data->defect_offset[pzone] - defect_offset;

    //Skip defect tracks
    while ((defect_amount != 0) && ((sys_data->defect_info[defect_offset] + track_zone_start) <= track))
    {
        track++;
        defect_offset++;
        defect_amount--;
    }

    return track | (head * 0x1000) | (block * 0x2000);
}

uint32_t PhysToLBA(const struct dd_disk* disk, uint16_t head, uint16_t track, uint16_t block)
{
    uint16_t expectedvalue = track | (head * 0x1000) | (block * 0x2000);

    for (uint16_t lba = 0; lba < SIZE_LBA; lba++)
    {
        if (disk->lba_phys_table[lba] == expectedvalue)
        {
            return lba;
        }
    }
    return 0xFFFF;
}



unsigned int get_zone_from_head_track(unsigned int head, unsigned int track)
{
    unsigned int zone;
    for (zone = 7; zone > 0; --zone) {
        if (track >= TrackZoneTable[0][zone]) {
            break;
        }
    }

    return zone + head;
}


static uint8_t* get_sector_base_mame(const struct dd_disk* disk,
    unsigned int head, unsigned int track, unsigned int block, unsigned int sector)
{
    static const unsigned int start_offset[] = {
        0x0000000, 0x05f15e0, 0x0b79d00, 0x10801a0,
        0x1523720, 0x1963d80, 0x1d414c0, 0x20bbce0,
        0x23196e0, 0x28a1e00, 0x2df5dc0, 0x3299340,
        0x36d99a0, 0x3ab70e0, 0x3e31900, 0x4149200
    };

    unsigned int zone = get_zone_from_head_track(head, track);

    /* Development disk have 0xC0 sector size for head == 0 && track < 6,
     * in all other cases use default sector size based on zone
     */
    unsigned int sector_size = (disk->development && head == 0 && track < 6)
        ? 0xC0
        : zone_sec_size_phys[zone];

    /* For the sake of tr_off computation we need zone - head */
    zone = zone - head;
    unsigned int tr_off = track - TrackZoneTable[0][zone];

    /* For start_offsets and all other macros we want (zone + head * 8) */
    zone += head * 8;

    /* compute sector offset */
    unsigned int offset = start_offset[zone]
        + tr_off * TRACKSIZE(zone)
        + block * BLOCKSIZE(zone)
        + sector * sector_size;

    /* Access to protected LBA should return an error */
    if (sector == 0 && track < (SYSTEM_LBAS / 2))
    {
        uint16_t lblock = offset / BLOCKSIZE(0);
        uint16_t lblock_sys = disk->offset_sys / BLOCKSIZE(0);
        uint16_t lblock_id = disk->offset_id / BLOCKSIZE(0);

        if ((lblock < PROTECT_LBA && lblock != lblock_sys)
         || (lblock > PROTECT_LBA && lblock < (DISKID_LBA + 2) && lblock != lblock_id)) {
            return NULL;
        }
    }

    return disk->istorage->data(disk->storage) + offset;
}

static uint8_t* get_sector_base_sdk(const struct dd_disk* disk,
    unsigned int head, unsigned int track, unsigned int block, unsigned int sector)
{
    uint16_t lba = PhysToLBA(disk, head, track, block);

    if (lba > MAX_LBA)
    {
        DebugMessage(M64MSG_ERROR, "Invalid LBA (Head:%d - Track:%04x - Block:%d)", head, track, block);
        return NULL;
    }

    /* Development disk have 0xC0 sector size for head == 0 && track < 6,
     * in all other cases use default sector size based on zone
     */
    unsigned int sector_size = (disk->development && head == 0 && track < 6)
        ? 0xC0
        : zone_sec_size_phys[get_zone_from_head_track(head, track)];
    const struct dd_sys_data* sys_data = (void*)(disk->istorage->data(disk->storage) + disk->offset_sys);
    unsigned int offset = LBAToByte(sys_data, 0, lba) + sector * sector_size;

    /* Handle Errors for wrong System Data */
    if (sector == 0 && lba < SYSTEM_LBAS)
    {
        uint16_t lblock = offset / BLOCKSIZE(0);
        uint16_t lblock_sys = disk->offset_sys / BLOCKSIZE(0);
        uint16_t lblock_id = disk->offset_id / BLOCKSIZE(0);

        if ((lblock < PROTECT_LBA && lblock != lblock_sys)
         || (lblock > PROTECT_LBA && lblock < (DISKID_LBA + 2) && lblock != lblock_id)) {
            return NULL;
        }
    }

    if (lba <= MAX_LBA && sector == 0)
        DebugMessage(M64MSG_VERBOSE, "LBA %d - Offset %08X - Size %04X", lba, offset, sector_size * SECTORS_PER_BLOCK);

    return disk->istorage->data(disk->storage) + offset;
}

static uint8_t* get_sector_base_d64(const struct dd_disk* disk,
    unsigned int head, unsigned int track, unsigned int block, unsigned int sector)
{
    const struct dd_sys_data* sys_data = (void*)(disk->istorage->data(disk->storage) + disk->offset_sys);

    uint16_t rom_lba_end = big16(sys_data->rom_lba_end);
    uint16_t ram_lba_start = big16(sys_data->ram_lba_start);
    uint16_t ram_lba_end = big16(sys_data->ram_lba_end);
    uint8_t disk_type = sys_data->type & 0x0F;
    unsigned int sector_size = zone_sec_size_phys[get_zone_from_head_track(head, track)];
    uint16_t lba = PhysToLBA(disk, head, track, block);
    unsigned int offset = 0;

    if (lba < DISKID_LBA)
    {
        //System Data
        offset = disk->offset_sys;
    }
    else if ((lba >= DISKID_LBA) && (lba < SYSTEM_LBAS))
    {
        //Disk ID
        offset = disk->offset_id;
    }
    else if (lba <= (rom_lba_end + SYSTEM_LBAS))
    {
        //ROM Area
        offset = D64_OFFSET_DATA + LBAToByteA(disk_type, SYSTEM_LBAS, lba - SYSTEM_LBAS) + (sector * sector_size);
    }
    else if (((lba - SYSTEM_LBAS) >= ram_lba_start) && ((lba - SYSTEM_LBAS) <= ram_lba_end))
    {
        //RAM Area
        offset = disk->offset_ram + LBAToByteA(disk_type, ram_lba_start + SYSTEM_LBAS, lba - SYSTEM_LBAS - ram_lba_start) + (sector * sector_size);
    }
    else
    {
        //Invalid
        DebugMessage(M64MSG_ERROR, "Invalid LBA (Head:%d - Track:%04x - Block:%d)", head, track, block);
        return NULL;
    }

    if (lba <= MAX_LBA && sector == 0)
        DebugMessage(M64MSG_VERBOSE, "LBA %d - Offset %08X - Size %04X", lba, offset, sector_size * SECTORS_PER_BLOCK);

    return disk->istorage->data(disk->storage) + offset;
}


uint8_t* get_sector_base(const struct dd_disk* disk,
    unsigned int head, unsigned int track, unsigned int block, unsigned int sector)
{
    switch(disk->format)
    {
    case DISK_FORMAT_MAME:
        return get_sector_base_mame(disk, head, track, block, sector);
    case DISK_FORMAT_SDK:
        return get_sector_base_sdk(disk, head, track, block, sector);
    case DISK_FORMAT_D64:
        return get_sector_base_d64(disk, head, track, block, sector);
    default:
        return NULL;
    }
}

uint8_t* scan_and_expand_disk_format(uint8_t* data, size_t size,
    unsigned int* format, unsigned int* development,
    size_t* offset_sys, size_t* offset_id, size_t* offset_ram, size_t* size_ram)
{
    /* Search for good System Data */
    const unsigned int blocks[8] = { 0, 1, 2, 3, 8, 9, 10, 11 };
    int isValidDisk = -1;
    int isValidDiskID = -1;
    unsigned int isDevelopment = 0;

    for (int i = 0; i < 8; i++)
    {
        uint32_t offset = BLOCKSIZE(0) * blocks[i];
        const struct dd_sys_data* sys_data = (void*)(&data[offset]);

        if ((offset + 0x20) >= size || (size < MAME_FORMAT_DUMP_SIZE && size < SDK_FORMAT_DUMP_SIZE && i > 0))
        {
            isValidDisk = -1;
            break;
        }

        //Disk Type
        if ((sys_data->type & 0xEF) > 6) continue;

        //IPL Load Block
        uint16_t ipl_load_blk = big16(sys_data->ipl_load_blk);
        if (ipl_load_blk > (MAX_LBA - SYSTEM_LBAS) || ipl_load_blk == 0x0000) continue;

        //IPL Load Address
        uint32_t ipl_load_addr = big32(sys_data->ipl_load_addr);
        if (ipl_load_addr < 0x80000000 && ipl_load_addr >= 0x80800000) continue;

        //Country Code
        uint32_t disk_region = big32(sys_data->region);
        switch (disk_region)
        {
        case DD_REGION_JP:
        case DD_REGION_US:
        case DD_REGION_DV:
            isValidDisk = i;
            break;
        default:
            continue;
        }

        //Verify if sector repeats
        if (size == MAME_FORMAT_DUMP_SIZE || size == SDK_FORMAT_DUMP_SIZE)
        {
            uint8_t sectorsize = SECTORSIZE_SYS;

            //Development System Data
            if (blocks[i] == 2 || blocks[i] == 3 || blocks[i] == 10 || blocks[i] == 11)
                sectorsize = SECTORSIZE_SYS_DEV;

            for (int j = 1; j < SECTORS_PER_BLOCK; j++)
            {
                if (memcmp(&data[offset + ((j - 1) * sectorsize)], &data[offset + (j * sectorsize)], sectorsize) != 0)
                {
                    isValidDisk = -1;
                    break;
                }
                else
                {
                    isValidDisk = i;
                }
            }
        }

        if (isValidDisk != -1)
            break;
    }

    if (isValidDisk == 2 || isValidDisk == 3 || isValidDisk == 10 || isValidDisk == 11)
        isDevelopment = 1;

    if (isValidDisk == -1)
    {
        DebugMessage(M64MSG_ERROR, "Invalid DD Disk System Data.");
        return NULL;
    }

    const uint16_t RAM_START_LBA[7] = { 0x5A2, 0x7C6, 0x9EA, 0xC0E, 0xE32, 0x1010, 0x10DC };
    const uint32_t RAM_SIZES[7] = { 0x24A9DC0, 0x1C226C0, 0x1450F00, 0xD35680, 0x6CFD40, 0x1DA240, 0x0 };

    if (size == MAME_FORMAT_DUMP_SIZE || size == SDK_FORMAT_DUMP_SIZE)
    {
        //Check Disk ID
        //Verify if sector repeats
        for (int i = 14; i < 16; i++)
        {
            for (int j = 1; j < SECTORS_PER_BLOCK; j++)
            {
                if (memcmp(&data[(i * BLOCKSIZE(0)) + (j - 1) * SECTORSIZE_SYS], &data[(i * BLOCKSIZE(0)) + j * SECTORSIZE_SYS], SECTORSIZE_SYS) != 0)
                {
                    isValidDiskID = -1;
                    break;
                }
                else
                {
                    isValidDiskID = i;
                }
            }

            if (isValidDiskID != -1)
                break;
        }

        if (isValidDiskID == -1)
        {
            DebugMessage(M64MSG_ERROR, "Invalid DD Disk ID Data.");
            return NULL;
        }
    }
    else
    {
        //Check D64 File Size
        const struct dd_sys_data* sys_data = (void*)(&data[D64_OFFSET_SYS]);

        uint16_t rom_lba_end = big16(sys_data->rom_lba_end);
        uint16_t ram_lba_start = big16(sys_data->ram_lba_start);
        uint16_t ram_lba_end = big16(sys_data->ram_lba_end);
        uint8_t disk_type = sys_data->type & 0x0F;

        size_t rom_size = LBAToByteA(disk_type, 24, rom_lba_end + 1);
        size_t ram_size = 0;
        if (ram_lba_start != 0xFFFF && ram_lba_end != 0xFFFF)
            ram_size = LBAToByteA(disk_type, SYSTEM_LBAS + ram_lba_start, ram_lba_end + 1 - ram_lba_start);

        size_t d64_size = D64_OFFSET_DATA + rom_size + ram_size;
        size_t full_d64_size = D64_OFFSET_DATA + rom_size + RAM_SIZES[disk_type];

        DebugMessage(M64MSG_INFO, "D64 Disk Areas - ROM: 0000 - %04X / RAM: %04X - %04X", rom_lba_end, ram_lba_start, ram_lba_end);

        if (ram_lba_start != (RAM_START_LBA[disk_type] - 24) && ram_lba_start != 0xFFFF)
        {
            isValidDisk = -1;
            DebugMessage(M64MSG_ERROR, "Invalid D64 Disk RAM Start Info (expected %04X)", (RAM_START_LBA[disk_type] - SYSTEM_LBAS));
        }
        else if (size != d64_size)
        {
            isValidDisk = -1;
            DebugMessage(M64MSG_ERROR, "Invalid D64 Disk size %zu (calculated 0x200 + 0x%zx + 0x%zx = %zu).", size, rom_size, ram_size, d64_size);
        }
        else
        {
            //Change Size to fit all of RAM Area possible
            uint8_t* buffer = malloc(full_d64_size);
            if (buffer == NULL) {
                DebugMessage(M64MSG_ERROR, "Failed to allocate memory for D64 disk dump");
                return NULL;
            }
            memset(buffer, 0, full_d64_size);
            memcpy(buffer, data, d64_size);

            struct dd_sys_data* sys_data_ = (void*)(&buffer[D64_OFFSET_SYS]);

            //Modify System Data so there are no errors in emulation
            sys_data_->format = 0x10;
            sys_data_->type |= 0x10;
            if (disk_type < 6)
            {
                sys_data_->ram_lba_start = big16((RAM_START_LBA[disk_type] - SYSTEM_LBAS));
                sys_data_->ram_lba_end = big16(MAX_LBA - SYSTEM_LBAS);
            }
            else
            {
                sys_data_->ram_lba_start = 0xFFFF;
                sys_data_->ram_lba_end = 0xFFFF;
            }

            /* FIXME: you're not supposed to know that data was malloc'ed */
            free(data);
            data = buffer;
            size = full_d64_size;
        }
    }


    switch (size)
    {
    case MAME_FORMAT_DUMP_SIZE:
        *format = DISK_FORMAT_MAME;
        *development = isDevelopment;
        *offset_sys = 0x4D08 * isValidDisk;
        *offset_id = 0x4D08 * isValidDiskID;
        break;

    case SDK_FORMAT_DUMP_SIZE: {
        *format = DISK_FORMAT_SDK;
        *development = isDevelopment;
        *offset_sys = 0x4D08 * isValidDisk;
        *offset_id = 0x4D08 * isValidDiskID;
        const struct dd_sys_data* sys_data = (void*)(&data[*offset_sys]);
        *offset_ram = LBAToByteA(sys_data->type & 0xF, 0, RAM_START_LBA[sys_data->type & 0xF]);
        *size_ram = RAM_SIZES[sys_data->type & 0xF];
        } break;

    default:
        if (isValidDisk == -1)
        {
            DebugMessage(M64MSG_ERROR, "Invalid DD Disk size %zu.", size);
            return NULL;
        }
        else
        {
            //D64
            *format = DISK_FORMAT_D64;
            *development = 1;
            *offset_sys = 0x000;
            *offset_id = 0x100;
            const struct dd_sys_data* sys_data = (void*)(&data[*offset_sys]);
            *offset_ram = D64_OFFSET_DATA + LBAToByteA(sys_data->type & 0xF, 24, big16(sys_data->rom_lba_end) + 1);
            *size_ram = RAM_SIZES[sys_data->type & 0xF];
        }
    }

    return data;
}


const char* get_disk_format_name(unsigned int format)
{
    switch(format)
    {
    case DISK_FORMAT_MAME:
        return "MAME";
    case DISK_FORMAT_SDK:
        return "SDK";
    case DISK_FORMAT_D64:
        return "D64";
    default:
        return "<unknown>";
    }
}
