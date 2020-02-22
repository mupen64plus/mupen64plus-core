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
#include <zlib.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backends/api/storage_backend.h"
#include "device/dd/dd_controller.h"
#include "main/util.h"

int open_file_storage(struct file_storage* fstorage, size_t size, const char* filename)
{
    /* ! Take ownership of filename ! */
    fstorage->filename = filename;
    fstorage->size = size;

    /* allocate memory for holding data */
    fstorage->data = malloc(fstorage->size);
    if (fstorage->data == NULL) {
        return -1;
    }

    /* try to load storage file content */
    return read_from_file(fstorage->filename, fstorage->data, fstorage->size);
}

int open_rom_file_storage(struct file_storage_rom* fstorage, const char* filename, const char* save_filename, unsigned int max_size_bytes)
{
    fstorage->data = NULL;
    fstorage->size = 0;
    fstorage->filename = NULL;
    int gz_read_sucess = 0;

    if (save_filename != NULL) {
        gzFile gz_file;
        gz_file = gzopen(save_filename, "rb");

        fstorage->data = malloc(max_size_bytes);

        if (fstorage->data != NULL) {
            gz_read_sucess = gzread(gz_file, fstorage->data, max_size_bytes);
            fstorage->size = (size_t)gz_read_sucess;
        }
    }

    file_status_t err = file_ok;

    if (save_filename == NULL || gz_read_sucess <= 0) {
        err = load_file(filename, (void**)&fstorage->data, &fstorage->size);
    }

    if (err == file_ok) {
        /* ! take ownsership of filename ! */
        fstorage->filename = filename;
        fstorage->saveto_filename = save_filename;
    }

    return err;
}

void close_file_storage(struct file_storage* fstorage)
{
    free((void*)fstorage->data);
    free((void*)fstorage->filename);
}

void close_rom_file_storage(struct file_storage_rom* fstorage)
{
    free((void*)fstorage->data);
    free((void*)fstorage->filename);
    free((void*)fstorage->saveto_filename);
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

static uint8_t* file_storage_rom_data(const void* storage)
{
    struct file_storage_rom* fstorage = (struct file_storage_rom*)storage;
    return fstorage->data;
}

static size_t file_storage_rom_size(const void* storage)
{
    struct file_storage_rom* fstorage = (struct file_storage_rom*)storage;
    return fstorage->size;
}

static void file_storage_save(void* storage)
{
    struct file_storage* fstorage = (struct file_storage*)storage;

    switch(write_to_file(fstorage->filename, fstorage->data, fstorage->size))
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

static void file_storage_parent_save(void* storage)
{
    struct file_storage* fstorage = (struct file_storage*)((struct file_storage*)storage)->filename;
    file_storage_save(fstorage);
}

static void file_storage_dd_sdk_dump_save(void* storage)
{
    static uint8_t sdk_buffer[SDK_FORMAT_DUMP_SIZE];
    struct file_storage_rom* fstorage = (struct file_storage_rom*)storage;

    dd_convert_to_sdk(fstorage->data, sdk_buffer);

    file_status_t err;

    gzFile gz_file;
    gz_file = gzopen(fstorage->saveto_filename, "wb");
    int gzres = gzwrite(gz_file, sdk_buffer, SDK_FORMAT_DUMP_SIZE);


    if ((gzres < 0) || ((size_t)gzres != SDK_FORMAT_DUMP_SIZE)){
        DebugMessage(M64MSG_ERROR, "Failed to write 64DD save file");
    }

    gzclose(gz_file);
}

const struct storage_backend_interface g_ifile_storage =
{
    file_storage_data,
    file_storage_size,
    file_storage_save
};


const struct storage_backend_interface g_ifile_storage_ro =
{
    file_storage_rom_data,
    file_storage_rom_size,
    NULL
};

const struct storage_backend_interface g_isubfile_storage =
{
    file_storage_data,
    file_storage_size,
    file_storage_parent_save
};

const struct storage_backend_interface g_ifile_storage_dd_sdk_dump =
{
    file_storage_rom_data,
    file_storage_rom_size,
    file_storage_dd_sdk_dump_save
};
