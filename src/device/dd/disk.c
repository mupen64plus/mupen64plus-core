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

static void storage_disk_save(void* storage)
{
    struct dd_disk* disk = (struct dd_disk*)storage;

    // XXX: you have now access to all disk members
    // and can handle the various format specificities here

    disk->istorage->save(disk->storage);
}

const struct storage_backend_interface g_istorage_disk =
{
    storage_disk_data,
    storage_disk_size,
    storage_disk_save
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
    uint32_t vzone, pzone = 0;

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



static unsigned int seek_track_mame(const struct dd_disk* disk,
    uint32_t cur_tk, uint32_t cur_sec, uint32_t host_secbyte,
    unsigned char bm_write, unsigned char bm_block,
    unsigned int* bm_zone, unsigned int* bm_track_offset)
{
    static const unsigned int start_offset[] = {
        0x0000000, 0x05f15e0, 0x0b79d00, 0x10801a0,
        0x1523720, 0x1963d80, 0x1d414c0, 0x20bbce0,
        0x23196e0, 0x28a1e00, 0x2df5dc0, 0x3299340,
        0x36d99a0, 0x3ab70e0, 0x3e31900, 0x4149200
    };

    static const unsigned int tracks[] = {
        0x000, 0x09e, 0x13c, 0x1d1, 0x266, 0x2fb, 0x390, 0x425
    };

    unsigned int error = 0;
    unsigned int tr_off;
    unsigned int head_x_8 = ((cur_tk & 0x1000) >> 9);
    unsigned int track = (cur_tk & 0x0fff);

    /* find track bm_zone */
    for (*bm_zone = 7; *bm_zone > 0; --*bm_zone) {
        if (track >= tracks[*bm_zone]) {
            break;
        }
    }

    tr_off = track - tracks[*bm_zone];

    /* set zone and track offset */
    *bm_zone += head_x_8;
    *bm_track_offset = start_offset[*bm_zone] + tr_off * TRACKSIZE(*bm_zone)
        + bm_block * BLOCKSIZE(*bm_zone)
        + (cur_sec - bm_write) * zone_sec_size[*bm_zone];

    if (cur_sec == 0)
    {
        uint16_t block = *bm_track_offset / BLOCKSIZE(0);
        uint16_t block_sys = disk->offset_sys / BLOCKSIZE(0);
        uint16_t block_id = disk->offset_id / BLOCKSIZE(0);

        if ((block < PROTECT_LBA && block != block_sys)
         || (block > PROTECT_LBA && block < (DISKID_LBA + 2) && block != block_id)) {
            error = 1;
        }
    }

    return error;
}

static unsigned int seek_track_sdk(const struct dd_disk* disk,
    uint32_t cur_tk, uint32_t cur_sec, uint32_t host_secbyte,
    unsigned char bm_write, unsigned char bm_block,
    unsigned int* bm_zone, unsigned int* bm_track_offset)
{
    unsigned int error = 0;
    uint16_t head = ((cur_tk & 0x1000) / 0x1000);
    uint16_t track = (cur_tk & 0x0fff);
    uint16_t block = bm_block;
    uint16_t sector = cur_sec - bm_write;
    uint16_t sectorsize = host_secbyte + 1;
    uint16_t lba = PhysToLBA(disk, head, track, block);

    if (lba > MAX_LBA)
    {
        error = 1;
        DebugMessage(M64MSG_ERROR, "Invalid LBA (Head:%d - Track:%04x - Block:%d)", head, track, block);
        return error;
    }

    const struct dd_sys_data* sys_data = (void*)(disk->istorage->data(disk->storage) + disk->offset_sys);
    //*bm_zone = LBAToVZone(sys_data, lba);
    *bm_track_offset = LBAToByte(sys_data, 0, lba) + sector * sectorsize;

    //Handle Errors for wrong System Data
    if (sector == 0)
    {
        uint16_t block = *bm_track_offset / BLOCKSIZE(0);
        uint16_t block_sys = disk->offset_sys / BLOCKSIZE(0);
        uint16_t block_id = disk->offset_id / BLOCKSIZE(0);

        if ((block < PROTECT_LBA && block != block_sys)
         || (block > PROTECT_LBA && block < (DISKID_LBA + 2) && block != block_id)) {
            error = 1;
        }
    }

    if (lba <= MAX_LBA && sector == 0)
        DebugMessage(M64MSG_VERBOSE, "LBA %d - Offset %08X - Size %04X", lba, *bm_track_offset, sectorsize * SECTORS_PER_BLOCK);

    return error;
}

static unsigned int seek_track_d64(const struct dd_disk* disk,
    uint32_t cur_tk, uint32_t cur_sec, uint32_t host_secbyte,
    unsigned char bm_write, unsigned char bm_block,
    unsigned int* bm_zone, unsigned int* bm_track_offset)
{
    unsigned int error = 0;
    const struct dd_sys_data* sys_data = (void*)(disk->istorage->data(disk->storage) + disk->offset_sys);

    uint16_t rom_lba_end = big16(sys_data->rom_lba_end);
    uint16_t ram_lba_start = big16(sys_data->ram_lba_start);
    uint16_t ram_lba_end = big16(sys_data->ram_lba_end);
    uint8_t disk_type = sys_data->type & 0x0F;

    uint16_t head = ((cur_tk & 0x1000) / 0x1000);
    uint16_t track = (cur_tk & 0x0fff);
    uint16_t block = bm_block;
    uint16_t sector = cur_sec - bm_write;
    uint16_t sectorsize = host_secbyte + 1;
    uint16_t lba = PhysToLBA(disk, head, track, block);

    if (lba < DISKID_LBA)
    {
        //System Data
        *bm_track_offset = disk->offset_sys;
    }
    else if ((lba >= DISKID_LBA) && (lba < SYSTEM_LBAS))
    {
        //Disk ID
        *bm_track_offset = disk->offset_id;
    }
    else if (lba <= (rom_lba_end + SYSTEM_LBAS))
    {
        //ROM Area
        *bm_track_offset = D64_OFFSET_DATA + LBAToByteA(disk_type, SYSTEM_LBAS, lba - SYSTEM_LBAS) + (sector * sectorsize);
    }
    else if (((lba - SYSTEM_LBAS) >= ram_lba_start) && ((lba - SYSTEM_LBAS) <= ram_lba_end))
    {
        //RAM Area
        *bm_track_offset = D64_OFFSET_DATA + LBAToByteA(disk_type, SYSTEM_LBAS, rom_lba_end + 1);
        *bm_track_offset += LBAToByteA(disk_type, ram_lba_start + SYSTEM_LBAS, lba - SYSTEM_LBAS - ram_lba_start) + (sector * sectorsize);
    }
    else
    {
        //Invalid
        error = 1;
        *bm_track_offset = 0xFFFFFFFF;
        DebugMessage(M64MSG_ERROR, "Invalid LBA (Head:%d - Track:%04x - Block:%d)", head, track, block);
    }

    if (lba <= MAX_LBA && sector == 0)
        DebugMessage(M64MSG_VERBOSE, "LBA %d - Offset %08X - Size %04X", lba, *bm_track_offset, sectorsize * SECTORS_PER_BLOCK);

    return error;
}


unsigned int disk_seek_track(const struct dd_disk* disk,
    uint32_t cur_tk, uint32_t cur_sec, uint32_t host_secbyte,
    unsigned char bm_write, unsigned char bm_block,
    unsigned int* bm_zone, unsigned int* bm_track_offset)
{
    unsigned int error;

    if (disk->format == DISK_FORMAT_MAME)
    {
        error = seek_track_mame(disk,
            cur_tk, cur_sec, host_secbyte,
            bm_write, bm_block,
            bm_zone, bm_track_offset);
    }
    else if (disk->format == DISK_FORMAT_SDK)
    {
        error = seek_track_sdk(disk,
            cur_tk, cur_sec, host_secbyte,
            bm_write, bm_block,
            bm_zone, bm_track_offset);
    }
    else //if (disk->format == DISK_FORMAT_D64)
    {
        error = seek_track_d64(disk,
            cur_tk, cur_sec, host_secbyte,
            bm_write, bm_block,
            bm_zone, bm_track_offset);
    }

    return error;
}
