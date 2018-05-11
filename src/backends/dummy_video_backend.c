/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dummy_video_backend.c                                   *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2018 Bobby Smiles                                       *
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

#include "backends/dummy_video_backend.h"
#include "backends/api/video_backend.h"

#include "api/m64p_types.h"

static m64p_error dummy_video_open(void* vin, unsigned int width, unsigned int height)
{
    return M64ERR_SUCCESS;
}

static void dummy_video_close(void* vin)
{
}

static m64p_error dummy_grab_image(void* vin, void* data)
{
    return M64ERR_UNSUPPORTED;
}

const struct video_input_backend_interface g_idummy_video_input_backend =
{
    dummy_video_open,
    dummy_video_close,
    dummy_grab_image
};
