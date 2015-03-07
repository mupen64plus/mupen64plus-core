/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - audio_backend.h                                         *
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

/* This file contains the definitions for the audio backend functions which
 * will be called from other Core modules.
 */

#if !defined(M64P_API_AUDIO_BACKEND_H)
#define M64P_API_AUDIO_BACKEND_H

#include "m64p_types.h"

#include <stddef.h>


/* Thin wrappers to ease usage of backend callbacks - used by ai_controller.c */
void set_audio_format(struct m64p_audio_backend* backend, unsigned int frequency, unsigned int bits);
void push_audio_samples(struct m64p_audio_backend* backend, const void* buffer, size_t size);

#endif /* M64P_API_AUDIO_BACKEND_H */
