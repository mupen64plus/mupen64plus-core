/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*   Mupen64plus - disk.h                                                  *
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

#ifndef M64P_DEVICE_DISK_H
#define M64P_DEVICE_DISK_H

#include <stdint.h>
#include <stddef.h>

struct storage_backend_interface;

/* Disk format sizes */
#define MAME_FORMAT_DUMP_SIZE 0x0435b0c0
#define SDK_FORMAT_DUMP_SIZE  0x03dec800

#define DD_REGION_JP UINT32_C(0xe848d316)
#define DD_REGION_US UINT32_C(0x2263ee56)
#define DD_REGION_DV UINT32_C(0x00000000)

#define DISK_FORMAT_MAME      0
#define DISK_FORMAT_SDK       1
#define DISK_FORMAT_D64       2

/* System Data struct */
struct dd_sys_data
{
    uint32_t region;
    uint8_t format;
    uint8_t type;
    uint16_t ipl_load_blk;
    uint8_t defect_offset[16];
    uint32_t dummy;
    uint32_t ipl_load_addr;
    uint8_t defect_info[192];
    uint16_t rom_lba_end;
    uint16_t ram_lba_start;
    uint16_t ram_lba_end;
    uint16_t dummy2;
};

/* D64 */
#define D64_OFFSET_SYS      0x000
#define D64_OFFSET_ID       0x100
#define D64_OFFSET_DATA     0x200

/* disk geometry definitions */
enum { SECTORS_PER_BLOCK = 85 };
enum { BLOCKS_PER_TRACK = 2 };

enum { DD_DISK_SYSTEM_DATA_SIZE = 0xe8 };

static const unsigned int zone_sec_size[16] = {
    232, 216, 208, 192, 176, 160, 144, 128,
    216, 208, 192, 176, 160, 144, 128, 112
};
static const unsigned int zone_sec_size_phys[9] = {
    232, 216, 208, 192, 176, 160, 144, 128, 112
};

static const uint32_t ZoneTracks[16] = {
    158, 158, 149, 149, 149, 149, 149, 114,
    158, 158, 149, 149, 149, 149, 149, 114
};
static const uint32_t DiskTypeZones[7][16] = {
    { 0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10 },
    { 0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11 },
    { 0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12 },
    { 0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13 },
    { 0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8 }
};
static const uint32_t RevDiskTypeZones[7][16] = {
    { 0, 1, 2, 5, 6, 7, 8, 9, 4, 3, 15, 14, 13, 12, 11, 10 },
    { 0, 1, 2, 3, 7, 8, 9, 10, 6, 5, 4, 15, 14, 13, 12, 11 },
    { 0, 1, 2, 3, 4, 9, 10, 11, 8, 7, 6, 5, 15, 14, 13, 12 },
    { 0, 1, 2, 3, 4, 5, 11, 12, 10, 9, 8, 7, 6, 15, 14, 13 },
    { 0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8 }
};
static const uint32_t StartBlock[7][16] = {
    { 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1 },
    { 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0 },
    { 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1 },
    { 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0 },
    { 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1 },
    { 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 },
    { 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1 }
};
static const uint16_t VZoneLBATable[7][16] = {
    {0x0124, 0x0248, 0x035A, 0x047E, 0x05A2, 0x06B4, 0x07C6, 0x08D8, 0x09EA, 0x0AB6, 0x0B82, 0x0C94, 0x0DA6, 0x0EB8, 0x0FCA, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x06A2, 0x07C6, 0x08D8, 0x09EA, 0x0AFC, 0x0BC8, 0x0C94, 0x0DA6, 0x0EB8, 0x0FCA, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x0690, 0x07A2, 0x08C6, 0x09EA, 0x0AFC, 0x0C0E, 0x0CDA, 0x0DA6, 0x0EB8, 0x0FCA, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x0690, 0x07A2, 0x08B4, 0x09C6, 0x0AEA, 0x0C0E, 0x0D20, 0x0DEC, 0x0EB8, 0x0FCA, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x0690, 0x07A2, 0x08B4, 0x09C6, 0x0AD8, 0x0BEA, 0x0D0E, 0x0E32, 0x0EFE, 0x0FCA, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x0690, 0x07A2, 0x086E, 0x0980, 0x0A92, 0x0BA4, 0x0CB6, 0x0DC8, 0x0EEC, 0x1010, 0x10DC},
    {0x0124, 0x0248, 0x035A, 0x046C, 0x057E, 0x0690, 0x07A2, 0x086E, 0x093A, 0x0A4C, 0x0B5E, 0x0C70, 0x0D82, 0x0E94, 0x0FB8, 0x10DC}
};
static const uint16_t TrackZoneTable[2][8] = {
    {0x000, 0x09E, 0x13C, 0x1D1, 0x266, 0x2FB, 0x390, 0x425},
    {0x091, 0x12F, 0x1C4, 0x259, 0x2EE, 0x383, 0x418, 0x48A}
};

#define SECTORSIZE(_zone) zone_sec_size[_zone]
#define BLOCKSIZE(_zone) SECTORSIZE(_zone) * SECTORS_PER_BLOCK
#define TRACKSIZE(_zone) BLOCKSIZE(_zone) * BLOCKS_PER_TRACK
#define ZONESIZE(_zone) TRACKSIZE(_zone) * ZoneTracks[_zone]
#define VZONESIZE(_zone) TRACKSIZE(_zone) * (ZoneTracks[_zone] - 0xC)
#define VZoneToPZone(x, y) DiskTypeZones[y][x]

#define MAX_LBA             0x10DB
#define SIZE_LBA            MAX_LBA+1
#define SYSTEM_LBAS         24
#define DISKID_LBA          14
#define PROTECT_LBA         12

#define SECTORSIZE_SYS SECTORSIZE(0)
#define SECTORSIZE_SYS_DEV SECTORSIZE(3)

struct dd_disk
{
    void* storage;
    const struct storage_backend_interface* istorage;
    uint16_t lba_phys_table[0x10DC];
    uint8_t format;
    uint8_t development;
    size_t offset_sys;
    size_t offset_id;
    size_t offset_ram;
};

/* Storage interface which handles the various 64DD disks format specificities */
extern const struct storage_backend_interface g_istorage_disk;

/* Disk Helper routines */
void GenerateLBAToPhysTable(struct dd_disk* disk);
uint32_t LBAToVZone(const struct dd_sys_data* sys_data, uint32_t lba);
uint32_t LBAToVZoneA(uint8_t type, uint32_t lba);
uint32_t LBAToByte(const struct dd_sys_data* sys_data, uint32_t lba, uint32_t nlbas);
uint32_t LBAToByteA(uint8_t type, uint32_t lba, uint32_t nlbas);
uint16_t LBAToPhys(const struct dd_sys_data* sys_data, uint32_t lba);
uint32_t PhysToLBA(const struct dd_disk* disk, uint16_t head, uint16_t track, uint16_t block);

unsigned int get_zone_from_head_track(unsigned int head, unsigned int track);

uint8_t* get_sector_base(const struct dd_disk* disk,
    unsigned int head, unsigned int track, unsigned int block, unsigned int sector);

uint8_t* scan_and_expand_disk_format(uint8_t* data, size_t size,
    unsigned int* format, unsigned int* development,
    size_t* offset_sys, size_t* offset_id, size_t* offset_ram, size_t* size_ram);


const char* get_disk_format_name(unsigned int format);

#endif
