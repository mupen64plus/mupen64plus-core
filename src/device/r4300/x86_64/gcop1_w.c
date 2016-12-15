/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - gcop1_w.c                                               *
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

#include <stdio.h>

#include "assemble.h"
#include "interpret.h"
#include "device/r4300/cp1.h"
#include "device/r4300/ops.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"

#if defined(COUNT_INSTR)
#include "device/r4300/instr_counters.h"
#endif

void gencvt_s_w(void)
{
#if defined(COUNT_INSTR)
   inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_S_W
   gencallinterp((unsigned long long)cached_interpreter_table.CVT_S_W, 0);
#else
   gencheck_cop1_unusable();
   mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
   fild_preg64_dword(RAX);
   mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fd]));
   fstp_preg64_dword(RAX);
#endif
}

void gencvt_d_w(void)
{
#if defined(COUNT_INSTR)
   inc_m32rel(&instr_count[117]);
#endif
#ifdef INTERPRET_CVT_D_W
   gencallinterp((unsigned long long)cached_interpreter_table.CVT_D_W, 0);
#else
   gencheck_cop1_unusable();
   mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_simple())[g_dev.r4300.recomp.dst->f.cf.fs]));
   fild_preg64_dword(RAX);
   mov_xreg64_m64rel(RAX, (unsigned long long *)(&(r4300_cp1_regs_double())[g_dev.r4300.recomp.dst->f.cf.fd]));
   fstp_preg64_qword(RAX);
#endif
}

