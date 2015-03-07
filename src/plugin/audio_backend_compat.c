/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - audio_backend_compat.c                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "audio_backend_compat.h"

#include "api/m64p_types.h"
#include "ai/ai_controller.h"
#include "main/main.h"
#include "main/rom.h"
#include "plugin/plugin.h"

#include <stddef.h>
#include <stdint.h>


/* A fully compliant implementation is not really possible with just the zilmar spec.
 * We assume bits == 16 (assumption compatible with audio-sdl plugin implementation)
 */
static void set_audio_format_via_audio_plugin(void* user_data, unsigned int frequency, unsigned int bits)
{
    /* save registers values */
    uint32_t saved_ai_dacrate = g_ai.regs[AI_DACRATE_REG];

    /* notify plugin of the new frequency (can't do the same for bits) */
    g_ai.regs[AI_DACRATE_REG] = (ROM_PARAMS.aidacrate / frequency) - 1;
    audio.aiDacrateChanged(ROM_PARAMS.systemtype);

    /* restore original registers values */
    g_ai.regs[AI_DACRATE_REG] = saved_ai_dacrate;
}

/* Abuse core & audio plugin implementation details to obtain the desired effect. */
static void push_audio_samples_via_audio_plugin(void* user_data, const void* buffer, size_t size)
{
    /* save registers values */
    uint32_t saved_ai_length = g_ai.regs[AI_LEN_REG];
    uint32_t saved_ai_dram = g_ai.regs[AI_DRAM_ADDR_REG];

    /* notify plugin of new samples to play.
     * Exploit the fact that buffer points in g_rdram to retreive dram_addr_reg value */
    g_ai.regs[AI_DRAM_ADDR_REG] = (uint8_t*)buffer - (uint8_t*)g_rdram;
    g_ai.regs[AI_LEN_REG] = size;
    audio.aiLenChanged();

    /* restore original registers vlaues */
    g_ai.regs[AI_LEN_REG] = saved_ai_length;
    g_ai.regs[AI_DRAM_ADDR_REG] = saved_ai_dram;
}


const struct m64p_audio_backend AUDIO_BACKEND_COMPAT =
{
    NULL,
    set_audio_format_via_audio_plugin,
    push_audio_samples_via_audio_plugin
};
