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

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/rdp/rdp_core.h"
#include "device/ri/ri_controller.h"
#include "plugin/plugin.h"

#include <string.h>

void poweron_fb(struct fb* fb)
{
    memset(fb, 0, sizeof(*fb));
    fb->once = 1;
    fb->read_address_counter = 0;
}


static void pre_framebuffer_read(struct fb* fb, uint32_t address)
{
    if (!fb->infos[0].addr) return;

    size_t i, j;

    for(i = 0; i < FB_INFOS_COUNT; ++i)
    {
        if (fb->infos[i].addr)
        {
            unsigned int start = fb->infos[i].addr;
            unsigned int end = start + fb->infos[i].width*
                               fb->infos[i].height*
                               fb->infos[i].size - 1;
            if (address >= start && address <= end)
            {
                for (j = 0; j < fb->read_address_counter; ++j)
                {
                    if (address >= fb->read_address[j] && address < fb->read_address[j] + 0x1000)
                        return;
                }
                gfx.fBRead(address);
                if (fb->read_address_counter == FB_READ_ADDRESS_COUNT)
                    fb->read_address_counter = 0;
                fb->read_address[fb->read_address_counter++] = address;
            }
        }
    }
}

static void pre_framebuffer_write(struct fb* fb, uint32_t address, uint32_t length)
{
    if (!fb->infos[0].addr) return;

    size_t i;

    for(i = 0; i < FB_INFOS_COUNT; ++i)
    {
        if (fb->infos[i].addr)
        {
            unsigned int start = fb->infos[i].addr;
            unsigned int end = start + fb->infos[i].width*
                               fb->infos[i].height*
                               fb->infos[i].size - 1;
            if (address >= start && address <= end)
                gfx.fBWrite(address, length);
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

    uint32_t addr, length;
    switch (mask)
    {
    case 0x000000ff:
        addr = address;
        length = 1;
        break;

    case 0x0000ff00:
        addr = address + 1;
        length = 1;
        break;

    case 0x0000ffff:
        addr = address;
        length = 2;
        break;

    case 0x00ff0000:
        addr = address + 2;
        length = 1;
        break;

    case 0xff000000:
        addr = address + 3;
        length = 1;
        break;

    case 0xffff0000:
        addr = address + 2;
        length = 2;
        break;

    case 0xffffffff:
        addr = address;
        length = 4;
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Unknown mask %x in write_rdram_fb", mask);
        write_rdram_dram(dp->ri, address, value, mask);
        return;
    }

    pre_framebuffer_write(&dp->fb, addr, length);
    write_rdram_dram(dp->ri, address, value, mask);
}


#define R(x) read_ ## x
#define W(x) write_ ## x
#define RW(x) R(x), W(x)

void protect_framebuffers(struct rdp_core* dp)
{
    struct fb* fb = &dp->fb;
    struct mem_handler fb_handler = { dp, RW(rdram_fb) };

    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite)
        gfx.fBGetFrameBufferInfo(fb->infos);

    if (!fb->infos[0].addr) return;

    size_t i;
    fb->read_address_counter = 0;

    for(i = 0; i < FB_INFOS_COUNT; ++i)
    {
        if (fb->infos[i].addr)
        {
            int j;
            int start = fb->infos[i].addr;
            int end = start + fb->infos[i].width*
                      fb->infos[i].height*
                      fb->infos[i].size - 1;
            start >>= 16;
            end >>= 16;
            for (j=start; j<=end; j++)
            {
                map_region(dp->r4300->mem, 0x0000+j, M64P_MEM_RDRAM, &fb_handler);
            }

            /* disable "fast memory" if framebuffer handlers are used */
            if (fb->once != 0)
            {
                fb->once = 0;
                dp->r4300->recomp.fast_memory = 0;
                invalidate_r4300_cached_code(dp->r4300, 0, 0);
            }
        }
    }
}

void unprotect_framebuffers(struct rdp_core* dp)
{
    struct fb* fb = &dp->fb;
    struct mem_handler ram_handler = { dp->ri, RW(rdram_dram) };

    if (!fb->infos[0].addr) return;

    size_t i;

    for(i = 0; i < FB_INFOS_COUNT; ++i)
    {
        if (fb->infos[i].addr)
        {
            int j;
            int start = fb->infos[i].addr;
            int end = start + fb->infos[i].width*
                      fb->infos[i].height*
                      fb->infos[i].size - 1;
            start = start >> 16;
            end = end >> 16;

            for (j=start; j<=end; j++)
            {
                map_region(dp->r4300->mem, 0x0000+j, M64P_MEM_RDRAM, &ram_handler);
            }
        }
    }
}
