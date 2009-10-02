/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cheat.h                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#ifndef CHEAT_H
#define CHEAT_H

#include "util.h" // list_t

#define ENTRY_BOOT 0
#define ENTRY_VI 1

extern list_t g_Cheats;

void cheat_apply_cheats(int entry);
void cheat_read_config(void);
void cheat_write_config(void);
void cheat_delete_all(void);
void cheat_load_current_rom(void);
void cheat_unload_current_rom(void);

// represents all cheats for a given rom
typedef struct rom_cheats {
    char *rom_name;
    unsigned int crc1;
    unsigned int crc2;
    list_t cheats; // list of cheat_t's
} rom_cheats_t;

typedef struct cheat_option {
    int code; /* e.g. 0xFF */
    char *description; /* e.g. Music Off */
} cheat_option_t;

typedef struct cheat_code {
    unsigned int address;
    int value;
    int old_value;
    int option;     // If this is an option mark it with 0x1, else 0x0
} cheat_code_t;

typedef struct cheat {
    char *name;
    char *comment;
    int number;
    int enabled;
    int always_enabled;
    int was_enabled;
    list_t cheat_codes;
    list_t options;
} cheat_t;

list_t cheats_for_current_rom(); // use cheats_free to free returned list

void cheats_free(list_t *cheats); // list_t becomes invalid after this!

rom_cheats_t *cheat_new_rom(void);
cheat_t *cheat_new_cheat(rom_cheats_t *);
cheat_code_t *cheat_new_cheat_code(cheat_t *);
void cheat_delete_rom(rom_cheats_t *);
void cheat_delete_cheat(rom_cheats_t *, cheat_t *);
void cheat_delete_cheat_code(cheat_t *, cheat_code_t *);

#endif // #define CHEAT_H
