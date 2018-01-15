/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cheat.h                                                 *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2008 Okaygo                                             *
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

#ifndef M64P_MAIN_CHEAT_H
#define M64P_MAIN_CHEAT_H

#define ENTRY_BOOT 0
#define ENTRY_VI 1

struct m64p_cheat_code;

void cheat_apply_cheats(int entry);

void cheat_init(void);
void cheat_uninit(void);
int cheat_add_new(const char* name, m64p_cheat_code* code_list, int num_codes);
int cheat_set_enabled(const char* name, int enabled);
void cheat_delete_all(void);
int cheat_add_hacks(void);

#endif
