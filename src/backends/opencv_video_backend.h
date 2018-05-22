/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - opencv_video_backend.h                                  *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2017 Bobby Smiles                                       *
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

#ifndef M64P_BACKENDS_OPENCV_VIDEO_BACKEND_H
#define M64P_BACKENDS_OPENCV_VIDEO_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "backends/api/video_backend.h"

struct opencv_video_backend
{
    char* device;
    unsigned int width;
    unsigned int height;

    /* using void* to avoid leaking C++ stuff in this header */
    void* cap;
};

#if 0
void cv_imshow(const char* name, unsigned int width, unsigned int height, int channels, void* data);
#endif

extern const struct video_input_backend_interface g_iopencv_video_input_backend;

#ifdef __cplusplus
}
#endif

#endif
