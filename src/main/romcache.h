/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - romcache.h                                              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
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

#ifndef __ROMCACHE_H__
#define __ROMCACHE_H__

#include <limits.h> 
#include <time.h>
#include "md5.h"

#define COMMENT_MAXLENGTH 256

/* The romdatabase contains the items mupen64plus indexes for each rom. These
 * include the goodname (from the GoodN64 project), the current status of the rom
 * in mupen, the N64 savetype used in the original cartridge (often necessary for
 * booting the rom in mupen), the number of players (including netplay options),
 * and whether the rom can make use of the N64's rumble feature. Md5, crc1, and
 * crc2 used for rom lookup. Md5s are unique hashes of the ENTIRE rom. Crcs are not
 * unique and read from the rom header, meaning corrupt crcs are also a problem.
 * Crcs were widely used (mainly in the cheat system). Refmd5s allows for a smaller
 * database file and need not be used outside database loading.
 */
typedef struct
{
   char* goodname;
   md5_byte_t md5[16];
   md5_byte_t* refmd5;
   unsigned int crc1;
   unsigned int crc2;
   unsigned char status; /* Rom status on a scale from 0-5. */
   unsigned char savetype;
   unsigned char players; /* Local players 0-4, 2/3/4 way Netplay indicated by 5/6/7. */
   unsigned char rumble; /* 0 - No, 1 - Yes boolean for rumble support. */
} romdatabase_entry;

typedef struct _romdatabase_search
{
    romdatabase_entry entry;
    struct _romdatabase_search* next_entry;
    struct _romdatabase_search* next_crc;
    struct _romdatabase_search* next_md5;
} romdatabase_search;

typedef struct
{
    char* comment;
    romdatabase_search* crc_lists[256];
    romdatabase_search* md5_lists[256];
    romdatabase_search* list;
} _romdatabase;

extern romdatabase_entry empty_entry;

void romdatabase_open(void);
void romdatabase_close(void);
romdatabase_entry* ini_search_by_md5(md5_byte_t* md5);
/* Should be used by current cheat system (isn't), when cheat system is
 * migrated to md5s, will be fully depreciated.
 */
romdatabase_entry* ini_search_by_crc(unsigned int crc1, unsigned int crc2);

#ifndef NO_GUI
/* See http://code.google.com/p/mupen64plus/wiki/RomBrowserColumns for details. */
typedef struct _cache_entry
{
    md5_byte_t md5[16];
    time_t timestamp;
    char filename[PATH_MAX];
    char usercomments[COMMENT_MAXLENGTH]; 
    char internalname[81]; /* Needs to be 4 times +1 (for '\0') for UTF8 conversion. */
    unsigned char compressiontype; /* Enum for compression type. */
    unsigned char imagetype; /* Enum for original rom image type. */
    unsigned char cic; /* N64 boot chip, determined from rom. */
    unsigned short countrycode; /* Found in rom header. */
    unsigned int archivefile; /* 0 indexed file number in multirom archive. */
    unsigned int crc1;
    unsigned int crc2;
    int romsize;
    romdatabase_entry* inientry; /* Persistent pointer to romdatabase.  */
    struct _cache_entry* next;
} cache_entry;

enum
{
    RCS_INIT = 1,
    RCS_RESCAN,
    RCS_SLEEP,
    RCS_BUSY,
    RCS_SHUTDOWN,
    RCS_WRITE_CACHE /* For quickly saving user comments from GUI frontend. */
};

typedef struct
{
    unsigned int length; 
    unsigned char rcstask;  /* Enum for what rcs thread should do. */
    unsigned char rcspause; /* Bool for pausing after last file, toggled by start/stopping emulation. */
    unsigned char clear;    /* Bool for clearing on an update. */
    cache_entry* top;
    cache_entry* last;
} rom_cache;

extern rom_cache g_romcache;

int rom_cache_system(void* _arg);
#endif /* NO_GUI */

#endif /* __ROMCACHE_H__ */

