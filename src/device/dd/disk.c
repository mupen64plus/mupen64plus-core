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

#include "backends/api/storage_backend.h"

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
