/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - file_storage.c                                          *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2016 Bobby Smiles                                       *
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

#include "file_storage.h"

#include <stdlib.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backends/api/storage_backend.h"
#include "device/dd/dd_controller.h"
#include "main/util.h"
#include "main/netplay.h"

int open_file_storage(struct file_storage* fstorage, size_t size, const char* filename)
{
    /* ! Take ownership of filename ! */
    fstorage->filename = filename;
    fstorage->size = size;
    fstorage->first_access = 1;

    /* allocate memory for holding data */
    fstorage->data = malloc(fstorage->size);
    if (fstorage->data == NULL) {
        return -1;
    }

    /* try to load storage file content */
    if (!netplay_is_init())
    {
        return read_from_file(fstorage->filename, fstorage->data, fstorage->size);
    }
    else
    {
        return netplay_read_storage(fstorage->filename, fstorage->data, fstorage->size);
    }
}

int open_rom_file_storage(struct file_storage* fstorage, const char* filename)
{
    fstorage->data = NULL;
    fstorage->size = 0;
    fstorage->filename = NULL;
    fstorage->first_access = 1;

    file_status_t err = load_file(filename, (void**)&fstorage->data, &fstorage->size);

    if (err == file_ok) {
        /* ! take ownsership of filename ! */
        fstorage->filename = filename;
    }

    return err;
}

void close_file_storage(struct file_storage* fstorage)
{
    free((void*)fstorage->data);
    free((void*)fstorage->filename);
}


static uint8_t* file_storage_data(const void* storage)
{
    struct file_storage* fstorage = (struct file_storage*)storage;
    return fstorage->data;
}

static size_t file_storage_size(const void* storage)
{
    struct file_storage* fstorage = (struct file_storage*)storage;
    return fstorage->size;
}

static void file_storage_save(void* storage, size_t start, size_t size)
{
    if (netplay_is_init() && netplay_get_controller(0) == -1)
        return;

    struct file_storage* fstorage = (struct file_storage*)storage;

    file_status_t err;

    /* On first save access ignore start/size and write full storage content,
     * otherwise write only updated chunk */
    if (fstorage->first_access) {
        fstorage->first_access = 0;
        err = write_to_file(fstorage->filename, fstorage->data, fstorage->size);
    }
    else {
        err = write_chunk_to_file(fstorage->filename, fstorage->data + start, size, start);
    }

    switch(err)
    {
    case file_open_error:
        DebugMessage(M64MSG_WARNING, "couldn't open storage file '%s' for writing", fstorage->filename);
        break;
    case file_write_error:
        DebugMessage(M64MSG_WARNING, "failed to write storage file '%s'", fstorage->filename);
        break;
    default:
        break;
    }
}

static void file_storage_parent_save(void* storage, size_t start, size_t size)
{
    struct file_storage* fstorage = (struct file_storage*)((struct file_storage*)storage)->filename;
    file_storage_save(fstorage, start, size);
}

static void dummy_save(void* storage, size_t start, size_t size)
{
    /* do nothing */
}


const struct storage_backend_interface g_ifile_storage =
{
    file_storage_data,
    file_storage_size,
    file_storage_save
};


const struct storage_backend_interface g_ifile_storage_ro =
{
    file_storage_data,
    file_storage_size,
    dummy_save
};

const struct storage_backend_interface g_isubfile_storage =
{
    file_storage_data,
    file_storage_size,
    file_storage_parent_save
};
