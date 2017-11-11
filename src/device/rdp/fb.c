/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - fb.c                                                    *
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

#include "fb.h"

#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/rdp/rdp_core.h"
#include "device/ri/ri_controller.h"
#include "plugin/plugin.h"

#include <string.h>

static inline size_t fb_buffer_size(const FrameBufferInfo* fb_info)
{
    return fb_info->width * fb_info->height * fb_info->size;
}

void poweron_fb(struct fb* fb)
{
    memset(fb, 0, sizeof(*fb));
    fb->once = 1;
}


static void pre_framebuffer_read(struct fb* fb, uint32_t address)
{
    size_t i;

    for (i = 0; i < FB_INFOS_COUNT; ++i) {

        /* skip empty fb info */
        if (fb->infos[i].addr == 0) {
            continue;
        }

        /* if address in within a fb and its page is dirty,
         * notify GFX plugin and mark page as not dirty */
        uint32_t begin = fb->infos[i].addr;
        uint32_t end   = fb->infos[i].addr + fb_buffer_size(&fb->infos[i]) - 1;

        if ((address >= begin) && (address <= end) && (fb->dirty_page[address >> 12])) {
            gfx.fBRead(address);
            fb->dirty_page[address >> 12] = 0;
        }
    }
}

static void post_framebuffer_write(struct fb* fb, uint32_t address)
{
    size_t i;

    for (i = 0; i < FB_INFOS_COUNT; ++i) {

        /* skip empty fb info */
        if (fb->infos[i].addr == 0) {
            continue;
        }

        /* if address in within a fb notify GFX plugin */
        uint32_t begin = fb->infos[i].addr;
        uint32_t end   = fb->infos[i].addr + fb_buffer_size(&fb->infos[i]) - 1;

        if ((address >= begin) && (address <= end)) {
            /* XXX: always assume full word access */
            gfx.fBWrite((address & ~0x3), 4);
        }
    }
}

void read_rdram_fb(void* opaque, uint32_t address, uint32_t* value)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    pre_framebuffer_read(&dp->fb, address);
    read_rdram_dram(dp->ri, address, value);
}

void write_rdram_fb(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    write_rdram_dram(dp->ri, address, value, mask);
    post_framebuffer_write(&dp->fb, address);
}


#define R(x) read_ ## x
#define W(x) write_ ## x
#define RW(x) R(x), W(x)

void protect_framebuffers(struct rdp_core* dp)
{
    size_t i, j;
    struct fb* fb = &dp->fb;
    struct mem_mapping fb_mapping = { 0, 0, M64P_MEM_RDRAM, { dp, RW(rdram_fb) } };

    /* check API support */
    if (!(gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite)) {
        return;
    }

    /* ask fb info to gfx plugin */
    gfx.fBGetFrameBufferInfo(fb->infos);

    /* return early if not FB info is present */
    if (fb->infos[0].addr == 0) {
        return;
    }

    for (i = 0; i < FB_INFOS_COUNT; ++i) {

        /* skip empty fb info */
        if (fb->infos[i].addr == 0) {
            continue;
        }

        /* map fb rw handlers */
        fb_mapping.begin = fb->infos[i].addr;
        fb_mapping.end   = fb->infos[i].addr + fb_buffer_size(&fb->infos[i]) - 1;
        apply_mem_mapping(dp->r4300->mem, &fb_mapping);

        /* mark all pages that are within a fb as dirty */
        for (j = fb_mapping.begin >> 12; j <= (fb_mapping.end >> 12); ++j) {
            fb->dirty_page[j] = 1;
        }

        /* disable dynarec "fast memory" code generation to avoid direct memory accesses */
        if (fb->once) {
            fb->once = 0;
            dp->r4300->recomp.fast_memory = 0;

            /* also need to invalidate cached code to regen non fast memory code path */
            invalidate_r4300_cached_code(dp->r4300, 0, 0);
        }
    }
}

void unprotect_framebuffers(struct rdp_core* dp)
{
    size_t i;
    struct fb* fb = &dp->fb;
    struct mem_mapping ram_mapping = { 0, 0, M64P_MEM_RDRAM, { dp->ri, RW(rdram_dram) } };

    /* return early if FB info is not supported or empty */
    if (!(gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite && fb->infos[0].addr)) {
        return;
    }

    for (i = 0; i < FB_INFOS_COUNT; ++i) {

        /* skip empty fb info */
        if (fb->infos[i].addr == 0) {
            continue;
        }

        /* restore ram rw handlers */
        ram_mapping.begin = fb->infos[i].addr;
        ram_mapping.end   = fb->infos[i].addr + fb_buffer_size(&fb->infos[i]) - 1;
        apply_mem_mapping(dp->r4300->mem, &ram_mapping);
    }
}
