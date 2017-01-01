/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - file_storage.c                                          *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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
#include "util.h"


int open_file_storage(struct file_storage* storage, size_t size, const char* filename)
{
    /* ! Take ownership of filename ! */
    storage->filename = filename;
    storage->size = size;

    /* allocate memory for holding data */
    storage->data = malloc(storage->size);
    if (storage->data == NULL) {
        return -1;
    }

    /* try to load storage file content */
    return read_from_file(storage->filename, storage->data, storage->size);
}

int open_rom_file_storage(struct file_storage* storage, const char* filename)
{
    storage->data = NULL;
    storage->size = 0;
    storage->filename = NULL;

    file_status_t err = load_file(filename, (void**)&storage->data, &storage->size);

    if (err == file_ok) {
        /* ! take ownsership of filename ! */
        storage->filename = filename;
    }

    return err;
}

void close_file_storage(struct file_storage* storage)
{
    free((void*)storage->data);
    free((void*)storage->filename);
}

void save_file_storage(void* opaque)
{
    struct file_storage* storage = (struct file_storage*)opaque;

    switch(write_to_file(storage->filename, storage->data, storage->size))
    {
    case file_open_error:
        DebugMessage(M64MSG_WARNING, "couldn't open storage file '%s' for writing", storage->filename);
        break;
    case file_write_error:
        DebugMessage(M64MSG_WARNING, "failed to write storage file '%s'", storage->filename);
        break;
    default:
        break;
    }
}
