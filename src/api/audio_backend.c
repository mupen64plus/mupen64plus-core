/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - audio_backend.c                                         *
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

#include "audio_backend.h"

#include "api/m64p_types.h"
#include "ai/ai_controller.h"

#include <string.h>

extern struct ai_controller g_ai;


/* Dummy Audio Backend object */
static void set_audio_format_dummy(void* user_data, unsigned int frequency, unsigned int bits)
{
}

static void push_audio_samples_dummy(void* user_data, const void* buffer, size_t size)
{
}

const struct m64p_audio_backend AUDIO_BACKEND_DUMMY =
{
    NULL,
    set_audio_format_dummy,
    push_audio_samples_dummy
};


/* Global function for use by frontend.c */
m64p_error SetAudioInterfaceBackend(unsigned int version, const struct m64p_audio_backend* backend)
{
    /* check input data */
    if (backend == NULL)
        return M64ERR_INPUT_ASSERT;

    /* check backend version */
    if (version != M64P_AUDIO_BACKEND_VERSION)
        return M64ERR_INCOMPATIBLE;

    /* if any of the function pointers are NULL, use the dummy audio backend */
    if (backend->set_audio_format == NULL ||
        backend->push_audio_samples == NULL)
    {
        backend = &AUDIO_BACKEND_DUMMY;
    }

    /* otherwise use the user provided backend */
    memcpy(&g_ai.backend, backend, sizeof(struct m64p_audio_backend));

    return M64ERR_SUCCESS;
}


/* Thin wrappers to ease usage of backend callbacks - used by ai_controller.c */
void set_audio_format(struct m64p_audio_backend* backend, unsigned int frequency, unsigned int bits)
{
    backend->set_audio_format(backend->user_data, frequency, bits);
}

void push_audio_samples(struct m64p_audio_backend* backend, const void* buffer, size_t size)
{
    backend->push_audio_samples(backend->user_data, buffer, size);
}
