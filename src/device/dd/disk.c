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
