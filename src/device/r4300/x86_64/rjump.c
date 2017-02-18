/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rjump.c                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2007 Richard Goedeken (Richard42)                       *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#include <stdlib.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/macros.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "device/r4300/x86_64/assemble_struct.h"

void dyna_jump(void)
{
    if (*r4300_stop() == 1)
    {
        dyna_stop();
        return;
    }

    if ((*r4300_pc_struct())->reg_cache_infos.need_map)
        *g_dev.r4300.return_address = (unsigned long long) ((*r4300_pc_struct())->reg_cache_infos.jump_wrapper);
    else
        *g_dev.r4300.return_address = (unsigned long long) (g_dev.r4300.cached_interp.actual->code + (*r4300_pc_struct())->local_addr);
}

void dyna_stop(void)
{
  if (g_dev.r4300.save_rip == 0)
    DebugMessage(M64MSG_WARNING, "Instruction pointer is 0 at dyna_stop()");
  else
  {
    *g_dev.r4300.return_address = (unsigned long long) g_dev.r4300.save_rip;
  }
}

