/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cheat.c                                                 *
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

// gameshark and xploder64 reference: http://doc.kodewerx.net/hacking_n64.html 

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <zlib.h> // TODO: compress cfg file

#include "../memory/memory.h"
#include "cheat.h"
#include "main.h"
#include "rom.h"
#include "util.h" // list utilities
#include "config.h"
#include "ini_reader.h"

// this seems stupid
#define CHEAT_CODE_MAGIC_VALUE 0xcafe // use this to know that old_value is uninitialized

#define DATABASE_FILENAME "mupen64plus.cht"
#define CHEAT_FILENAME "cheats.cfg"
static ini_file *cheat_file = NULL;
static ini_section *current_rom_section = NULL;
static list_t current_rom_cheats = NULL;

// for local cheats stored in cheats.cfg
list_t g_Cheats = NULL; // list of all supported cheats
static rom_cheats_t *g_Current = NULL; // current loaded rom

// private functions
static unsigned short read_address_16bit(unsigned int address)
{
    return *(unsigned short *)((rdramb + ((address & 0xFFFFFF)^S16)));
}

static unsigned char read_address_8bit(unsigned int address)
{
    return *(unsigned char *)((rdramb + ((address & 0xFFFFFF)^S8)));
}

static void update_address_16bit(unsigned int address, unsigned short new_value)
{
    *(unsigned short *)((rdramb + ((address & 0xFFFFFF)^S16))) = new_value;
}

static void update_address_8bit(unsigned int address, unsigned char new_value)
{
    *(unsigned short *)((rdramb + ((address & 0xFFFFFF)^S8))) = new_value;
}

static int address_equal_to_8bit(unsigned int address, unsigned char value)
{
    unsigned char value_read;
    value_read = *(unsigned short *)((rdramb + ((address & 0xFFFFFF)^S8)));
    return value_read == value;
}

static int address_equal_to_16bit(unsigned int address, unsigned short value)
{
    unsigned short value_read;
    value_read = *(unsigned short *)((rdramb + ((address & 0xFFFFFF)^S16)));
    return value_read == value;
}

// individual application - returns 0 if we are supposed to skip the next cheat
// (only really used on conditional codes)
static int execute_cheat(unsigned int address, unsigned short value, int *old_value)
{
    switch (address & 0xFF000000)
    {
        case 0x80000000:
        case 0x88000000:
        case 0xA0000000:
        case 0xA8000000:
        case 0xF0000000:
            // if pointer to old value is valid and uninitialized, write current value to it
            if(old_value && (*old_value == CHEAT_CODE_MAGIC_VALUE))
                *old_value = (int) read_address_8bit(address);
            update_address_8bit(address,value);
            return 1;
        case 0x81000000:
        case 0x89000000:
        case 0xA1000000:
        case 0xA9000000:
        case 0xF1000000:
            // if pointer to old value is valid and uninitialized, write current value to it
            if(old_value && (*old_value == CHEAT_CODE_MAGIC_VALUE))
                *old_value = (int) read_address_16bit(address);
            update_address_16bit(address,value);
            return 1;
        case 0xD0000000:
        case 0xD8000000:
            return address_equal_to_8bit(address,value);
        case 0xD1000000:
        case 0xD9000000:
            return address_equal_to_16bit(address,value);
        case 0xD2000000:
        case 0xDB000000:
            return !(address_equal_to_8bit(address,value));
        case 0xD3000000:
        case 0xDA000000:
            return !(address_equal_to_16bit(address,value));
        case 0xEE000000:
            // most likely, this doesnt do anything.
            execute_cheat(0xF1000318, 0x0040, NULL);
            execute_cheat(0xF100031A, 0x0000, NULL);
            return 1;
        default:
            return 1;
    }
}

static int gs_button_pressed(void)
{
    return key_pressed(config_get_number("Kbd Mapping GS Button", SDLK_g)) ||
           event_active(config_get_string("Joy Mapping GS Button", ""));
}

// public functions
void cheat_apply_cheats(int entry)
{
    list_t node1 = NULL;
    list_t node2 = NULL;
    cheat_code_t *code;
    
    if (current_rom_cheats == NULL)
        return;
    
    list_foreach(current_rom_cheats, node1) {
        cheat_t* cheat = (cheat_t*) node1->data;
        if(cheat->always_enabled || cheat->enabled)
        {
            cheat->was_enabled = 1;
            switch(entry)
            {
                case ENTRY_BOOT:
                    list_foreach(cheat->cheat_codes, node2)
                    {
                        code = (cheat_code_t *)node2->data;

                        // code should only be written once at boot time
                        if((code->address & 0xF0000000) == 0xF0000000)
                            execute_cheat(code->address, code->value, &code->old_value);
                    }
                    break;
                case ENTRY_VI:
                    list_foreach(cheat->cheat_codes, node2)
                    {
                        code = (cheat_code_t *)node2->data;

                        // conditional cheat codes
                        if((code->address & 0xF0000000) == 0xD0000000)
                        {
                            // if code needs GS button pressed and it's not, skip it
                            if(((code->address & 0xFF000000) == 0xD8000000 ||
                                (code->address & 0xFF000000) == 0xD9000000 ||
                                (code->address & 0xFF000000) == 0xDA000000 ||
                                (code->address & 0xFF000000) == 0xDB000000) &&
                               !gs_button_pressed())
                            {
                                // skip next code
                                if(node2->next != NULL)
                                    node2 = node2->next;
                                continue;
                            }

                            // if condition true, execute next cheat code
                            if(execute_cheat(code->address, code->value, NULL))
                            {
                                node2 = node2->next;
                                code = (cheat_code_t *)node2->data;

                                // if code needs GS button pressed, don't save old value
                                if(((code->address & 0xFF000000) == 0xD8000000 ||
                                    (code->address & 0xFF000000) == 0xD9000000 ||
                                    (code->address & 0xFF000000) == 0xDA000000 ||
                                    (code->address & 0xFF000000) == 0xDB000000))
                                   execute_cheat(code->address, code->value, NULL);
                                else
                                   execute_cheat(code->address, code->value, &code->old_value);
                            }
                            // if condition false, skip next code
                            else
                            {
                                if(node2->next != NULL)
                                    node2 = node2->next;
                                continue;
                            }
                        }
                        // GS button triggers cheat code
                        else if((code->address & 0xFF000000) == 0x88000000 ||
                                (code->address & 0xFF000000) == 0x89000000 ||
                                (code->address & 0xFF000000) == 0xA8000000 ||
                                (code->address & 0xFF000000) == 0xA9000000)
                        {
                            if(gs_button_pressed())
                                execute_cheat(code->address, code->value, NULL);
                        }
                        // normal cheat code
                        else
                        {
                            // exclude boot-time cheat codes
                            if((code->address & 0xF0000000) != 0xF0000000)
                                execute_cheat(code->address, code->value, &code->old_value);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        // if cheat was enabled, but is now disabled, restore old memory values
        else if(cheat->was_enabled)
        {
            cheat->was_enabled = 0;
            switch(entry)
            {
                case ENTRY_VI:
                    list_foreach(cheat->cheat_codes, node2)
                    {
                        code = (cheat_code_t *)node2->data;
              
                        // set memory back to old value and clear saved copy of old value
                        if(code->old_value != CHEAT_CODE_MAGIC_VALUE)
                        {
                            execute_cheat(code->address, code->old_value, NULL);
                            code->old_value = CHEAT_CODE_MAGIC_VALUE;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

/** cheat_read_config
 * First read and parse the DATABASE (PJ64),
 * then read and parse the CHEAT file.
*/
void cheat_read_config(void)
{

/**
 * Read and parse the DATABASE (PJ64),
*/
    char buf[PATH_MAX];
    snprintf(buf, PATH_MAX, "%s%s", get_installpath(), DATABASE_FILENAME);
    cheat_file = ini_file_parse(buf);

/**   Read config file and populate list of supported cheats. Format of cheat file is:
 *
 *   {Some Game's CRC}
 *   name=Some Game
 *
 *   [Cheat Name 1]
 *   enabled=1
 *   XXXXXXXX YYYY <-- cheat code (address, new value)
 *   XXXXXXXX YYYY
 *   XXXXXXXX YYYY
 * 
 *
 *   [Cheat Name 2]
 *   enabled=0
 *   XXXXXXXX YYYY
 *   XXXXXXXX YYYY
 *   XXXXXXXX YYYY
 *
 *   {Another Game's CRC}
 *   name=Another Game
 *   ...
 */
    char path[PATH_MAX];
    FILE *f = NULL;
    char line[2048];

    rom_cheats_t *romcheat = NULL;
    cheat_t *cheat = NULL;
    cheat_code_t *cheatcode = NULL;

    snprintf(path, PATH_MAX, "%s%s", get_configpath(), CHEAT_FILENAME);
    f = fopen(path, "r");

    // if no cheat config file installed, exit quietly
    if(!f) return;

    // parse file lines
    while(!feof(f))
    {
        if( !fgets( line, 2048, f ) )
            break;

        trim(line);

        if(strlen(line) == 0 ||
           line[0] == '#')     // comment
            continue;

        // beginning of new rom section
        if (line[0] == '{' && line[strlen(line)-1] == '}')
        {
            romcheat = cheat_new_rom();
            sscanf(line, "{%x %x}", &romcheat->crc1, &romcheat->crc2);
            continue;
        }

        // rom name (just informational)
        if(strncasecmp(line, "name=", 5) == 0)
        {
            romcheat->rom_name = strdup(strstr(line, "=")+1);
            continue;
        }

        // name of cheat
        if(line[0] == '[' && line[strlen(line)-1] == ']')
        {
            cheat = cheat_new_cheat(romcheat);
            line[strlen(line)-1] = '\0'; // get rid of trailing ']'
            cheat->name = strdup(line+1);
            continue;
        }

        // cheat always enabled?
        if(strncasecmp(line, "enabled=", 8) == 0)
        {
            sscanf(line, "enabled=%d", &cheat->always_enabled);
            continue;
        }

        // else, line must be a cheat code
        cheatcode = cheat_new_cheat_code(cheat);
        sscanf(line, "%x %x", &cheatcode->address, &cheatcode->value);
    }
    fclose(f);
}

/** cheat_write_config
 *   Write out all cheats to file
 */
void cheat_write_config(void)
{
    char path[PATH_MAX];
    FILE *f = NULL;

    list_node_t *node1, *node2, *node3;
    rom_cheats_t *romcheat = NULL;
    cheat_t *cheat = NULL;
    cheat_code_t *cheatcode = NULL;

    // if no cheats, don't bother writing out file
    if(list_empty(g_Cheats)) return;

    snprintf(path, PATH_MAX, "%s%s", get_configpath(), CHEAT_FILENAME);
    f = fopen(path, "w");
    if(!f)
        return;

    list_foreach(g_Cheats, node1)
    {
        romcheat = (rom_cheats_t *)node1->data;

        fprintf(f, "{%.8x %.8x}\n"
                "name=%s\n",
                romcheat->crc1,
                romcheat->crc2,
                romcheat->rom_name);

        list_foreach(romcheat->cheats, node2)
        {
            cheat = (cheat_t *)node2->data;

            fprintf(f, "\n[%s]\n", cheat->name);
            fprintf(f, "enabled=%d\n", cheat->always_enabled? 1 : 0);

            list_foreach(cheat->cheat_codes, node3)
            {
                cheatcode = (cheat_code_t *)node3->data;

                fprintf(f, "%.8x %.4x\n", cheatcode->address, cheatcode->value);
            }
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

void cheat_delete_all(void)
{
    ini_file_free(&cheat_file);
}

void cheat_unload_current_rom(void)
{
    current_rom_section = NULL;
    current_rom_cheats = NULL;
}

/** cheat_load_current_rom
 *   sets pointer to cheats for currently loaded rom.
 */
void cheat_load_current_rom(void)
{
    /* The title is always 22 chars long. Add one for the \0 */
    char buf[23];
    list_node_t *node = NULL;
    ini_section *section = NULL;

    if (ROM_HEADER && cheat_file)
    {
        snprintf(buf, 23, "%X-%X-C:%X",
                 g_MemHasBeenBSwapped ? sl(ROM_HEADER->CRC1) : ROM_HEADER->CRC1,
                 g_MemHasBeenBSwapped ? sl(ROM_HEADER->CRC2) : ROM_HEADER->CRC2,
                 (ROM_HEADER->Country_code)&0xff);
        list_foreach(cheat_file->sections, node)
        {
            section = node->data;
            if (strcasecmp(buf, section->title) == 0)
            {
                puts("FOUND CHEAT ENTRY");
                current_rom_section = section;
                break;
            }
        }
    }
}

static cheat_t *find_or_create_cheat(list_t *list, int number)
{
    list_t node = NULL;
    cheat_t *cheat = NULL;
    int found = 0;

    list_foreach(*list, node) {
        cheat = node->data;
        if (cheat->number == number) {
            found = 1;
            break;
        } else if (cheat->number == -1) {
            break;
        }
    }

    if (!found) {
        cheat = malloc(sizeof(cheat_t));
        cheat->name = NULL;
        cheat->comment = NULL;
        cheat->number = number;
        cheat->cheat_codes = NULL;
        cheat->options = NULL;
        cheat->enabled = 0;
        cheat->was_enabled = 0;
        cheat->always_enabled = 0;
        list_append(list, cheat);
    }

    return cheat;
}

list_t cheats_for_current_rom()
{
    list_t node = NULL;
    list_t node2 = NULL;
    list_t list = NULL;
    list_t temp = NULL;
    ini_entry *entry = NULL;
    cheat_t *cheat = NULL;
    rom_cheats_t *romcheat = NULL;
    int n = 0, len = 0;
    int value = 0;
    unsigned int address = 0;
    char buf[PATH_MAX];
    cheat_option_t *option;
    cheat_code_t *code;

    if (!current_rom_section)
    {
        return NULL;
    }

    list_foreach(current_rom_section->entries, node)
    {
        entry = node->data;
        len = strlen(entry->key);
        if (strcmp("_O", entry->key + len - 2) == 0) // Option for a cheat
        {
            if (sscanf(entry->key, "Cheat%d_O", &n) == 1)
            {
                cheat = find_or_create_cheat(&list, n);
                temp = tokenize_string(entry->value, ",");
                node2 = NULL;
                list_foreach(temp, node2)
                {
                    memset(buf, '\0', PATH_MAX);
                    if (sscanf(node2->data, "$%x %[a-zA-Z0-9 ']", &value, buf) == 2)
                    {
                        option = malloc(sizeof(cheat_option_t));
                        option->code = value;
                        option->description = malloc(strlen(buf) + 1);
                        strcpy(option->description, buf);
                        list_append(&cheat->options, option);
                    }
                    free(node2->data);
                }
                list_delete(&temp);
            }
        }
        else if (strcmp("_N", entry->key + len - 2) == 0) // Comment for a cheat
        {
            if (sscanf(entry->key, "Cheat%d_N", &n) == 1)
            {
                cheat = find_or_create_cheat(&list, n);
                cheat->comment = malloc(strlen(entry->value) + 1);
                strcpy(cheat->comment, entry->value);
            }
        }
        else if (sscanf(entry->key, "Cheat%d", &n) == 1)
        {
            cheat = find_or_create_cheat(&list, n);
            temp = tokenize_string(entry->value, ",");
            cheat->name = list_first_data(temp);
            list_node_delete(&temp, list_first_node(temp));
            node2 = NULL;
            list_foreach(temp, node2)
            {
                //TODO: This will not handle the case where all digits in code field
                // is '?'. Should be handled separately.
                if (sscanf(node2->data, "%x %x", &address, &value) == 2)
                {
                    code = malloc(sizeof(cheat_code_t));
                    code->address = address;

                    // If there i a '?' in the data, mark it with code->option = 1
                    if (strchr(node2->data, '?'))
                        code->option = 1;
                    else
                        code->option = 0;

                    code->value = value;
                    code->old_value = CHEAT_CODE_MAGIC_VALUE;
                    list_append(&cheat->cheat_codes, code);
                }
                free(node2->data);
            }
            list_delete(&temp);

            /* Remove quotation marks from around name */
            strcpy(cheat->name, cheat->name + 1);
            cheat->name[strlen(cheat->name) - 1] = '\0';
        }

        cheat = NULL;
    }

    // Adding cheats from cheats.cfg to the 'current_rom_cheats' list.
    rom_cheats_t *rom_cheat = NULL;
    code = NULL;
    node = NULL;
    node2 = NULL;
    cheat = NULL;
    unsigned int crc1, crc2;

    g_Current = NULL;
    crc1 = g_MemHasBeenBSwapped ? sl(ROM_HEADER->CRC1) : ROM_HEADER->CRC1;
    crc2 = g_MemHasBeenBSwapped ? sl(ROM_HEADER->CRC2) : ROM_HEADER->CRC2;

    list_foreach(g_Cheats, node) {
        rom_cheat = (rom_cheats_t *)node->data;

        if(crc1 == rom_cheat->crc1 &&
           crc2 == rom_cheat->crc2) {
            g_Current = rom_cheat;
            break;
        }
    }

    // if rom was found, clear any old saved values from cheat codes
    if(g_Current) {
        list_foreach(g_Current->cheats, node) {
            cheat = (cheat_t *)node->data;
            cheat_t* newcheat;
            newcheat = find_or_create_cheat(&list, -1);
            memcpy(newcheat,cheat,sizeof(cheat_t));
            newcheat->cheat_codes = NULL;
            newcheat->options = NULL;

            if (cheat->cheat_codes)
            {
                list_foreach(cheat->cheat_codes, node2) {
                    code = (cheat_code_t *)node2->data;
                    cheat_code_t* newcode;
                    newcode = malloc(sizeof(cheat_code_t));
                    memcpy(newcode,code,sizeof(cheat_code_t));
                    newcode->old_value = CHEAT_CODE_MAGIC_VALUE;
                    list_append(&newcheat->cheat_codes, newcode);
                }
            }
        }
    }

    current_rom_cheats = list;
    return list;
}

void cheats_free(list_t *cheats)
{
    cheat_t *cheat = NULL;
    cheat_option_t *option = NULL;
    list_t node1, node2;
    node1 = node2 = NULL;

    list_foreach(*cheats, node1) {
        cheat = node1->data;
        
        free(cheat->name);
        free(cheat->comment);

        list_foreach(cheat->cheat_codes, node2) {
            free(node2->data);
        }
        list_delete(&cheat->cheat_codes);

        list_foreach(cheat->options, node2) {
            option = node2->data;
            free(option->description);
            free(option);
        }
        list_delete(&cheat->options);
    }

    list_delete(cheats);
}

/** cheat_new_rom
 *   creates a new rom_cheats_t structure, appends it to the global list and returns it.
 */
rom_cheats_t *cheat_new_rom(void)
{
    rom_cheats_t *romcheat = malloc(sizeof(rom_cheats_t));

    if(!romcheat)
        return NULL;

    memset(romcheat, 0, sizeof(rom_cheats_t));
    list_append(&g_Cheats, romcheat);

    return romcheat;
}

/** cheat_new_cheat
 *   creates a new cheat_t structure, appends it to the given rom_cheats_t struct and returns it.
 */
cheat_t *cheat_new_cheat(rom_cheats_t *romcheat)
{
    cheat_t *cheat = malloc(sizeof(cheat_t));

    if(!cheat)
        return NULL;

    memset(cheat, 0, sizeof(cheat_t));
    list_append(&romcheat->cheats, cheat);

    return cheat;
}

/** cheat_new_cheat_code
 *   creates a new cheat_code_t structure, appends it to the given cheat_t struct and returns it.
 */
cheat_code_t *cheat_new_cheat_code(cheat_t *cheat)
{
    cheat_code_t *code = malloc(sizeof(cheat_code_t));

    if(!code)
        return NULL;

    memset(code, 0, sizeof(cheat_code_t));
    code->old_value = CHEAT_CODE_MAGIC_VALUE; // initialize old_value
    list_append(&cheat->cheat_codes, code);

    return code;
}

/** cheat_delete_rom
 *   deletes given rom structure and removes it from the global list.
 */
void cheat_delete_rom(rom_cheats_t *romcheat)
{
    list_node_t *romnode, *node1, *node2;
    cheat_t *cheat;
    cheat_code_t *cheatcode;

    if(!romcheat)
        return;

    if(romcheat->rom_name)
        free(romcheat->rom_name);

    // remove any cheats associated with this rom
    list_foreach(romcheat->cheats, node1)
    {
        cheat = (cheat_t *)node1->data;

        if(cheat->name)
            free(cheat->name);

        // remove any codes associated with this cheat
        list_foreach(cheat->cheat_codes, node2)
        {
            cheatcode = (cheat_code_t *)node2->data;
            if(cheatcode)
                free(cheatcode);
        }
        list_delete(&cheat->cheat_codes);
        free(cheat);
    }
    list_delete(&romcheat->cheats);

    // locate node associated with rom
    romnode = list_find_node(g_Cheats, romcheat);

    // free rom and remove it from the list
    free(romcheat);
    list_node_delete(&g_Cheats, romnode);
}

/** cheat_delete_cheat
 *   deletes given cheat structure and removes it from the given rom's cheat list.
 */
void cheat_delete_cheat(rom_cheats_t *romcheat, cheat_t *cheat)
{
    list_node_t *cheatnode, *node;
    cheat_code_t *cheatcode;

    if(!cheat)
        return;

    if(cheat->name)
        free(cheat->name);

    // remove any codes associated with this cheat
    list_foreach(cheat->cheat_codes, node)
    {
        cheatcode = (cheat_code_t *)node->data;
        if(cheatcode)
            free(cheatcode);
    }
    list_delete(&cheat->cheat_codes);

    // locate node associated with cheat
    cheatnode = list_find_node(romcheat->cheats, cheat);

    // free cheat and remove it from the rom's cheat list
    free(cheat);
    list_node_delete(&romcheat->cheats, cheatnode);
}

/** cheat_delete_cheat_code
 *   deletes given cheat code structure and removes it from the given cheat's code list.
 */
void cheat_delete_cheat_code(cheat_t *cheat, cheat_code_t *cheatcode)
{
    list_node_t *codenode;

    if(!cheatcode)
        return;

    // locate node associated with cheat
    codenode = list_find_node(cheat->cheat_codes, cheatcode);

    // free cheat code and remove it from the cheat's code list
    free(cheatcode);
    list_node_delete(&cheat->cheat_codes, codenode);
}

