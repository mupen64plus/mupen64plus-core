/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - audio_out_backend.h                                     *
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

#ifndef M64P_BACKENDS_AUDIO_OUT_BACKEND_H
#define M64P_BACKENDS_AUDIO_OUT_BACKEND_H

#include <stddef.h>

struct audio_out_backend
{
    void* user_data;
    void (*set_format)(void*,unsigned int, unsigned int);
    void (*push_samples)(void*,const void*,size_t);
};

void audio_out_set_format(struct audio_out_backend* aout, unsigned int frequency, unsigned int bits);
void audio_out_push_samples(struct audio_out_backend* aout, const void* buffer, size_t size);

#endif
