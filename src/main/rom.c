/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rom.c                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <bzlib.h>
#include <zlib.h>

#include "zip/unzip.h"
#include "lzma/lzmadec.h"
#include "7zip/7zExtract.h"
#include "7zip/7zCrc.h"

#include "md5.h"
#include "rom.h"
#include "translate.h"
#include "main.h"
#include "util.h"

#include "../memory/memory.h"
#include "../osd/osd.h"

#define DEFAULT 16

#define CHUNKSIZE 1024*128 /* Read files 128KB at a time. */

_romdatabase g_romdatabase;
romdatabase_entry empty_entry;

/* Global loaded rom memory space. */
unsigned char* rom;
/* Global loaded rom size. */
int taille_rom;

/* TODO: Replace with a glocal cache_entry. */
rom_header* ROM_HEADER;
rom_settings ROM_SETTINGS;

/* Tests if a file is a valid N64 rom by checking the first 4 bytes. */
int is_valid_rom(unsigned char buffer[4])
{
    /* Test if rom is a native .z64 image with header 0x80371240. [ABCD] */
    if((buffer[0]==0x80)&&(buffer[1]==0x37)&&(buffer[2]==0x12)&&(buffer[3]==0x40))
        return 1;
    /* Test if rom is a byteswapped .v64 image with header 0x37804012. [BADC] */
    else if((buffer[0]==0x37)&&(buffer[1]==0x80)&&(buffer[2]==0x40)&&(buffer[3]==0x12))
        return 1;
    /* Test if rom is a wordswapped .n64 image with header  0x40123780. [DCBA] */
    else if((buffer[0]==0x40)&&(buffer[1]==0x12)&&(buffer[2]==0x37)&&(buffer[3]==0x80))
        return 1;
    else
        return 0;
}

/* If rom is a .v64 or .n64 image, byteswap or wordswap loadlength amount of
 * rom data to native .z64 before forwarding. Makes sure that data extraction
 * and MD5ing routines always deal with a .z64 image.
 */
void swap_rom(unsigned char* localrom, unsigned char* imagetype, int loadlength)
{
    unsigned char temp;
    int i;

    /* Btyeswap if .v64 image. */
    if(localrom[0]==0x37)
        {
        *imagetype = V64IMAGE;
        for (i = 0; i < loadlength; i+=2)
            {
            temp=localrom[i];
            localrom[i]=localrom[i+1];
            localrom[i+1]=temp;
            }
        }
    /* Wordswap if .n64 image. */
    else if(localrom[0]==0x40)
        {
        *imagetype = N64IMAGE;
        for (i = 0; i < loadlength; i+=4)
            {
            temp=localrom[i];
            localrom[i]=localrom[1+3];
            localrom[i+3]=temp;
            temp=localrom[i+1];
            localrom[i+1]=localrom[i+2];
            localrom[i+2]=temp;
            }
        }
    else
        *imagetype = Z64IMAGE;
}

/* Open a file and test if its an uncompressed rom, bzip2ed rom, lzma stream compressed
 * rom, or gzipped rom. If so, set compressiontype and load *loadlength of the rom
 * into the returned pointer. On failure return NULL.
 */
unsigned char* load_single_rom(const char* filename, int* romsize, unsigned char* compressiontype, int* loadlength)
{
    int i;
    unsigned short romread = 0;
    unsigned char buffer[4];
    unsigned char* localrom;

    FILE* romfile;
    /* Uncompressed roms. */
    romfile=fopen(filename, "rb");
    if(romfile!=NULL)
        {
        fread(buffer, 1, 4, romfile);
        if(is_valid_rom(buffer))
            {
            *compressiontype = UNCOMPRESSED;
            fseek(romfile, 0L, SEEK_END);
            *romsize=ftell(romfile);
            fseek(romfile, 0L, SEEK_SET);
            localrom = (unsigned char*)malloc(*loadlength*sizeof(unsigned char));
            if(localrom==NULL)
                {
                fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                return NULL;
                }
            fread(localrom, 1, *loadlength, romfile); 
            romread = 1;
            }
        if(romread==0)
            {
            /* Bzip2 roms. */
            BZFILE* bzromfile;
            int bzerror;
            fseek(romfile, 0L, SEEK_SET);
            bzromfile = BZ2_bzReadOpen(&bzerror, romfile, 0, 0, NULL, 0);
            if(bzerror==BZ_OK)
                BZ2_bzRead(&bzerror, bzromfile, buffer, 4);
            if(bzerror==BZ_OK)
                {
                if(is_valid_rom(buffer))
                    {
                    *compressiontype = BZIP2_COMPRESSION;
                    fseek(romfile, 0L, SEEK_SET);
                    bzromfile = BZ2_bzReadOpen(&bzerror, romfile, 0, 0, NULL, 0);
                    *romsize=0;
                    localrom=NULL;
                    for( i = 0; bzerror==BZ_OK; ++i )
                        {
                        localrom = (unsigned char*)realloc(localrom, (i+1)*CHUNKSIZE*sizeof(unsigned char));
                        if(localrom==NULL)
                           {
                           fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                           return NULL;
                           }
                        *romsize += BZ2_bzRead(&bzerror, bzromfile, localrom+(i*CHUNKSIZE), CHUNKSIZE); 
                        }
                    if(bzerror==BZ_STREAM_END)
                       {
                       localrom = (unsigned char*)realloc(localrom,*loadlength*sizeof(unsigned char));
                       romread = 1; 
                       }
                    else
                       free(localrom);
                    }
                }
            BZ2_bzReadClose(&bzerror, bzromfile);
            }
        if(romread==0)
            {
            /* LZMA roms. */
            fseek(romfile, 0L, SEEK_SET);
            int lzmastatus;
            lzmadec_stream stream;
            stream.lzma_alloc = NULL;
            stream.lzma_free = NULL;
            stream.opaque = NULL;
            stream.avail_in = 0;
            stream.next_in = NULL;

            /* Minimum size to get decoded blocks back is 45.
           LZMA has 13 byte headers, likely 32 byte internal buffer. */
            unsigned char* buffer_in = (unsigned char*)malloc(45*sizeof(unsigned char));
            unsigned char* buffer_out = (unsigned char*)malloc(45*128*sizeof(unsigned char));
            if(buffer_in==NULL||buffer_out==NULL||lzmadec_init(&stream)!=LZMADEC_OK)
                {
                fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                return NULL;
                }

            fread(buffer_in, sizeof(unsigned char), 45, romfile);

            stream.next_in = buffer_in;
            stream.avail_in = 45;
            stream.next_out = buffer_out;
            stream.avail_out = 45;

            lzmastatus = lzmadec_decode (&stream, (stream.avail_in==0));
            if(lzmastatus==LZMADEC_OK&&is_valid_rom(buffer_out))
                {
                *compressiontype = LZMA_COMPRESSION;
                int oldsize;
                *romsize = 45 - stream.avail_out;

                buffer_in = (unsigned char*)realloc(buffer_in,CHUNKSIZE*sizeof(unsigned char));
                buffer_out = (unsigned char*)realloc(buffer_out,CHUNKSIZE*128*sizeof(unsigned char));
                localrom = (unsigned char*)malloc(*romsize*sizeof(unsigned char));
                if(buffer_in==NULL||buffer_out==NULL||localrom==NULL)
                    {
                    fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                    return NULL;
                    }

                memcpy(localrom,buffer_out, *romsize);
                while(lzmastatus==LZMADEC_OK)
                    {
                    fread(buffer_in, sizeof(unsigned char), CHUNKSIZE, romfile);
                    stream.next_in = buffer_in;
                    stream.avail_in = CHUNKSIZE;
                    stream.next_out = buffer_out;
                    stream.avail_out = CHUNKSIZE*128;
                    lzmastatus = lzmadec_decode (&stream, (stream.avail_in==0));

                    oldsize = *romsize;
                    *romsize += CHUNKSIZE*128-stream.avail_out;

                    localrom = (unsigned char*)realloc(localrom,*romsize*sizeof(unsigned char));
                    if(localrom==NULL)
                        {
                        fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                        return NULL;
                        }
                    memcpy(localrom+oldsize,buffer_out,CHUNKSIZE*128-stream.avail_out);
                    }

                if(lzmastatus==LZMADEC_STREAM_END) 
                    {
                    lzmadec_end(&stream);
                    localrom = (unsigned char*)realloc(localrom,*loadlength*sizeof(unsigned char));
                    romread = 1;
                    }
                }
            free(buffer_in);
            free(buffer_out);
            }
        fclose(romfile);
        }

    /* Gzipped roms. */
    if(romread==0)
        {
        gzFile* gzromfile;
        gzromfile=gzopen(filename, "rb");
        if(gzromfile!=NULL)
            {
            gzread(gzromfile, buffer, 4);
            if(is_valid_rom(buffer))
                {
                *compressiontype = GZIP_COMPRESSION;
                gzseek(gzromfile, 0L, SEEK_SET);
                *romsize=0;
                localrom=NULL;
                for( i = 0; !gzeof(gzromfile); ++i )
                    {
                    localrom = (unsigned char*)realloc(localrom, (i+1)*CHUNKSIZE*sizeof(unsigned char));
                    if(localrom==NULL)
                       {
                       fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                       return NULL;
                       }
                    *romsize += gzread(gzromfile, localrom+(i*CHUNKSIZE), CHUNKSIZE);
                    }
                if(gzeof(gzromfile))
                       {
                       localrom = (unsigned char*)realloc(localrom,*loadlength*sizeof(unsigned char));
                       romread = 1; 
                       }
                    else
                       free(localrom);
                gzclose(gzromfile);
                }
            }
        }

    /* File invalid, or valid rom not found in file. */
    if(romread==0)
        return NULL;

    return localrom;
}

static int ask_bad(void)
{
        printf(tr("The rom you are trying to load is probably a bad dump!\n"
                  "Be warned that this will probably give unexpected results.\n"));

    return 1;
}

static int ask_hack(void)
{
        printf(tr("The rom you are trying to load is probably a hack!\n"
                  "Be warned that this will probably give unexpected results.\n"));

    return 1;
}

int open_rom(const char* filename, unsigned int archivefile)
{
    if(g_EmulatorRunning)
         {
         stopEmulation();
         }

    md5_state_t state;
    md5_byte_t digest[16];
    romdatabase_entry* entry;
    char buffer[PATH_MAX];
    unsigned char compressiontype, imagetype;
    int i;

    if(rom)
        free(rom);

    /* Clear Byte-swapped flag, since ROM is now deleted. */
    g_MemHasBeenBSwapped = 0;

    UInt32 blockIndex = 0xFFFFFFFF;
    Byte* outBuffer = NULL;
    size_t outBufferSize = 0;

    CrcGenerateTable();

    strncpy(buffer, filename, PATH_MAX-1);
    buffer[PATH_MAX-1] = 0;
    if ((rom=load_single_rom(filename, &taille_rom, &compressiontype, &taille_rom))==NULL)
        {
        error_message(tr("Couldn't load Rom!")); 
        return -1;
        }

    if(outBuffer)
        free(outBuffer);

    swap_rom(rom, &imagetype, taille_rom);

    compressionstring(compressiontype, buffer);
    printf("Compression: %s\n", buffer);

    imagestring(imagetype, buffer);
    printf("Imagetype: %s\n", buffer);

    printf("Rom size: %d bytes (or %d Mb or %d Megabits)\n",
    taille_rom, taille_rom/1024/1024, taille_rom/1024/1024*8);

    /* TODO: Replace the following validation code with fill_entry(). */

    /* Load rom settings and check if it's a good dump. */
    md5_init(&state);
    md5_append(&state, (const md5_byte_t*)rom, taille_rom);
    md5_finish(&state, digest);
    for ( i = 0; i < 16; ++i ) 
        sprintf(buffer+i*2, "%02X", digest[i]);
    buffer[32] = '\0';
    strcpy(ROM_SETTINGS.MD5, buffer);
    printf("MD5: %s\n", buffer);

    if(ROM_HEADER)
        free(ROM_HEADER);
    ROM_HEADER = malloc(sizeof(rom_header));
    if(ROM_HEADER==NULL)
        {
        fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
        return 0;
        }
    memcpy(ROM_HEADER, rom, sizeof(rom_header));
    trim((char*)ROM_HEADER->nom); /* Remove trailing whitespace from Rom name. */

    printf("%x %x %x %x\n", ROM_HEADER->init_PI_BSB_DOM1_LAT_REG,
                            ROM_HEADER->init_PI_BSB_DOM1_PGS_REG,
                            ROM_HEADER->init_PI_BSB_DOM1_PWD_REG,
                            ROM_HEADER->init_PI_BSB_DOM1_PGS_REG2);
    printf("ClockRate = %x\n", sl((unsigned int)ROM_HEADER->ClockRate));
    printf("Version: %x\n", sl((unsigned int)ROM_HEADER->Release));
    printf("CRC: %x %x\n", sl((unsigned int)ROM_HEADER->CRC1), sl((unsigned int)ROM_HEADER->CRC2));
    printf ("Name: %s\n", ROM_HEADER->nom);
    if(sl(ROM_HEADER->Manufacturer_ID) == 'N')
        printf ("Manufacturer: Nintendo\n");
    else
        printf("Manufacturer: %x\n", (unsigned int)(ROM_HEADER->Manufacturer_ID));
    printf("Cartridge_ID: %x\n", ROM_HEADER->Cartridge_ID);

    countrycodestring(ROM_HEADER->Country_code, buffer);
    printf("Country: %s\n", buffer);
    printf ("PC = %x\n", sl((unsigned int)ROM_HEADER->PC));

    if((entry=ini_search_by_md5(digest))==&empty_entry)
        {
        if((entry=ini_search_by_crc(sl(ROM_HEADER->CRC1),sl(ROM_HEADER->CRC2)))==&empty_entry)
            {
            strcpy(ROM_SETTINGS.goodname, (char*)ROM_HEADER->nom);
            strcat(ROM_SETTINGS.goodname, " (unknown rom)");
            printf("%s\n", ROM_SETTINGS.goodname);
            ROM_SETTINGS.eeprom_16kb = 0;
            return 0;
            }
        }

    unsigned short close = 0;
    char* s = entry->goodname;
    if(s!=NULL)
        {
        for ( i = strlen(s); i > 1; --i )
        if(i!=0)
            {
            if(s[i-1]=='['&&(s[i]=='T'||s[i]=='t'||s[i]=='h'||s[i]=='f'))
                {
                if(!ask_hack())
                    close = 1;
                }
            else if(s[i-1]=='['&&s[i]=='b')
                {
                if(!ask_bad())
                    close = 1;
                }
            }
        }

    if(close)
        {
        free(rom);
        rom = NULL;
        free(ROM_HEADER);
        ROM_HEADER = NULL;
        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Rom closed."));
        return -3;
        }

    strncpy(ROM_SETTINGS.goodname, entry->goodname, 255);
    ROM_SETTINGS.goodname[255] = '\0';

    if(entry->savetype==EEPROM_16KB)
        ROM_SETTINGS.eeprom_16kb = 1;
    printf("EEPROM type: %d\n", ROM_SETTINGS.eeprom_16kb);
    return 0;
}

int close_rom(void)
{
    if(g_EmulatorRunning)
        stopEmulation();

    if(ROM_HEADER)
        {
        free(ROM_HEADER);
        ROM_HEADER = NULL;
        }

    if(rom)
        {
        free(rom);
        rom = NULL;
        }
     else
        return -1;

    /* Clear Byte-swapped flag, since ROM is now deleted. */
    g_MemHasBeenBSwapped = 0;
    main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Rom closed."));

    return 0;
}

/********************************************************************************************/
/* INI Rom database functions */

/* Convert two letters representing hexidecimal to the appropriate value: 00->0 - FF->255. */
static unsigned char hexconvert(const char* bigraph)
{
    unsigned char returnvalue = 0;
    char character;
    int digits;

    for(digits = 2; digits > 0; --digits)
        {
        returnvalue = returnvalue << 4;
        character = *bigraph++ | 0x20;
        if(character>='0'&&character<='9')
            returnvalue += character - '0';
        else if(character>='a'&&character<='f')
            returnvalue += character - 'a' + 10;
        }

    return returnvalue;
}

/* Helper function, identify the space of a line before an = sign. */
static int split_property(char* string)
{
    int counter = 0;
    while (string[counter] != '=' && string[counter] != '\0')
        ++counter;
    if(string[counter]=='\0')
        return -1;
    string[counter] = '\0';
    return counter;
}

void romdatabase_open(void)
{
    gzFile gzfile;
    char buffer[256];
    romdatabase_search* search = NULL;
    romdatabase_entry* entry = NULL;

    int stringlength, totallength, namelength, index, counter, value;
    char hashtemp[3];
    hashtemp[2] = '\0';

    if(g_romdatabase.comment!=NULL)
        return;

    /* Setup empty_entry. */
    empty_entry.goodname = "";
    for(counter=0; counter<16; ++counter)
       empty_entry.md5[counter]=0;
    empty_entry.refmd5 = NULL;
    empty_entry.crc1 = 0;
    empty_entry.crc2 = 0;
    empty_entry.status = 0;
    empty_entry.savetype = DEFAULT;
    empty_entry.players = DEFAULT;
    empty_entry.rumble = DEFAULT;

    /* Open romdatabase. */
    char* pathname = (char*)malloc(PATH_MAX*sizeof(char));
    snprintf(pathname, PATH_MAX, "%s%s", get_installpath(), "mupen64plus.ini");

    /* printf("Database file: %s \n", pathname); */
    gzfile = gzopen(pathname, "rb");
    if(gzfile==NULL)
        {
        printf("Unable to open rom database.\n");
        free(pathname);
        return;
        }
    free(pathname);

    /* Move through opening comments, set romdatabase.comment to non-NULL
    to signal we have a database. */
    totallength = 0;
    do
        {
        gzgets(gzfile, buffer, 255);
        if(buffer[0]!='[')
            {
            stringlength=strlen(buffer);
            totallength+=stringlength;
            if(g_romdatabase.comment==NULL) 
                {
                g_romdatabase.comment = (char*)malloc(stringlength+2);
                snprintf(g_romdatabase.comment, stringlength, "%s", buffer);
                }
            else
                {
                g_romdatabase.comment = (char*)realloc(g_romdatabase.comment, totallength+2);
                snprintf(g_romdatabase.comment, totallength+1, "%s%s", g_romdatabase.comment, buffer);
                }
            }
        }
    while (buffer[0] != '[' && !gzeof(gzfile));

    /* Clear premade indices. */
    for(counter = 0; counter < 255; ++counter)
        g_romdatabase.crc_lists[counter] = NULL;
    for(counter = 0; counter < 255; ++counter)
        g_romdatabase.md5_lists[counter] = NULL;
    g_romdatabase.list = NULL;

    do
        {
        if(buffer[0]=='[')
            {
            if(g_romdatabase.list==NULL)
                {
                g_romdatabase.list = (romdatabase_search*)malloc(sizeof(romdatabase_search));
                g_romdatabase.list->next_entry = NULL;
                g_romdatabase.list->next_crc = NULL;
                g_romdatabase.list->next_md5 = NULL;
                search = g_romdatabase.list;
                }
            else
                {
                search->next_entry = (romdatabase_search*)malloc(sizeof(romdatabase_search));
                search = search->next_entry;
                search->next_entry = NULL;
                search->next_crc = NULL;
                search->next_md5 = NULL;
                }
            for (counter=0; counter < 16; ++counter)
              {
              hashtemp[0] = buffer[counter*2+1];
              hashtemp[1] = buffer[counter*2+2];
              search->entry.md5[counter] = hexconvert(hashtemp);
              }
            /* Index MD5s by first 8 bits. */
            index = search->entry.md5[0];
            if(g_romdatabase.md5_lists[index]==NULL)
                g_romdatabase.md5_lists[index] = search;
            else
                {
                romdatabase_search* aux = g_romdatabase.md5_lists[index];
                search->next_md5 = aux;
                g_romdatabase.md5_lists[index] = search;
                }
            search->entry.goodname = NULL;
            search->entry.refmd5 = NULL;
            search->entry.crc1 = 0;
            search->entry.crc2 = 0;
            search->entry.status = 0; /* Set default to 0 stars. */
            search->entry.savetype = DEFAULT;
            search->entry.rumble = DEFAULT; 
            search->entry.players = DEFAULT; 
            }
        else
            {
            stringlength = split_property(buffer);
            if(stringlength!=-1)
                {
                if(!strcmp(buffer, "GoodName"))
                    {
                    /* Get length of GoodName since we dynamically allocate. */
                    namelength = strlen(buffer+stringlength+1);
                    search->entry.goodname = (char*)malloc(namelength*sizeof(char));
                    /* Make sure we're null terminated. */
                    if(buffer[stringlength+namelength]=='\n'||buffer[stringlength+namelength]=='\r')
                        buffer[stringlength+namelength] = '\0';
                    snprintf(search->entry.goodname, namelength, "%s", buffer+stringlength+1);
                    /* printf("Name: %s, Length: %d\n", search->entry.goodname, namelength-1); */
                    }
                else if(!strcmp(buffer, "CRC"))
                    {
                    buffer[stringlength+19] = '\0';
                    sscanf(buffer+stringlength+10, "%X", &search->entry.crc2);
                    buffer[stringlength+9] = '\0';
                    sscanf(buffer+stringlength+1, "%X", &search->entry.crc1);
                    buffer[stringlength+3] = '\0';
                    index = hexconvert(buffer+stringlength+1);
                    /* Index CRCs by first 8 bits. */
                    if(g_romdatabase.crc_lists[index]==NULL)
                        g_romdatabase.crc_lists[index] = search;
                    else
                        {
                        romdatabase_search* aux = g_romdatabase.crc_lists[index];
                        search->next_crc = aux;
                        g_romdatabase.crc_lists[index] = search;
                        }
                    }
                else if(!strcmp(buffer, "RefMD5"))
                    {
                    /* If we have a refernce MD5, dynamically allocate. */
                    search->entry.refmd5 = (md5_byte_t*)malloc(16*sizeof(md5_byte_t));
                    for (counter=0; counter < 16; ++counter)
                        {
                        hashtemp[0] = buffer[stringlength+1+counter*2];
                        hashtemp[1] = buffer[stringlength+2+counter*2];
                        search->entry.refmd5[counter] = hexconvert(hashtemp);
                        }
                    /* Lookup reference MD5 and replace non-default entries. */
                    if((entry = ini_search_by_md5(search->entry.refmd5))!=&empty_entry)
                        {
                        /* printf("RefMD5: %s\n", aux->goodname); */
                        if(entry->savetype!=DEFAULT)
                            search->entry.savetype = entry->savetype;
                        if(entry->status!=0)
                            search->entry.status = entry->status;
                        if(entry->players!=DEFAULT)
                            search->entry.players = entry->players;
                        if(entry->rumble!=DEFAULT)
                            search->entry.rumble = entry->rumble;
                        }
                    }
                else if(!strcmp(buffer, "SaveType"))
                    {
                    if(!strncmp(buffer+stringlength+1, "Eeprom 4KB", 10))
                        search->entry.savetype = EEPROM_4KB;
                    else if(!strncmp(buffer+stringlength+1, "Eeprom 16KB", 10))
                        search->entry.savetype = EEPROM_16KB;
                    else if(!strncmp(buffer+stringlength+1, "SRAM", 4))
                        search->entry.savetype = SRAM;
                    else if(!strncmp(buffer+stringlength+1, "Flash RAM", 9))
                        search->entry.savetype = FLASH_RAM;
                    else if(!strncmp(buffer+stringlength+1, "Controller Pack", 15))
                        search->entry.savetype = CONTROLLER_PACK;
                    else if(!strncmp(buffer+stringlength+1, "None", 4))
                        search->entry.savetype = NONE;
                    }
                else if(!strcmp(buffer, "Status"))
                    {
                    value = (unsigned char)atoi(buffer+stringlength+1);
                    if(value>-1&&value<6)
                        search->entry.status = value;
                    }
                else if(!strcmp(buffer, "Players"))
                    {
                    value = (unsigned char)atoi(buffer+stringlength+1);
                    if(value>-1&&value<8)
                        search->entry.players = value;
                    }
                else if(!strcmp(buffer, "Rumble"))
                    {
                    if(!strncmp(buffer+stringlength+1, "Yes", 3))
                        search->entry.rumble = 1;
                    if(!strncmp(buffer+stringlength+1, "No", 2))
                        search->entry.rumble = 0;
                    }
                }
            }

        gzgets(gzfile, buffer, 255);
        }
   while (!gzeof(gzfile));

   gzclose(gzfile);
}

void romdatabase_close(void)
{
    if (g_romdatabase.comment == NULL)
        return;

    free(g_romdatabase.comment);

    while (g_romdatabase.list != NULL)
        {
        romdatabase_search* search = g_romdatabase.list->next_entry;
        if(g_romdatabase.list->entry.goodname)
            free(g_romdatabase.list->entry.goodname);
        if(g_romdatabase.list->entry.refmd5)
            free(g_romdatabase.list->entry.refmd5);
        free(g_romdatabase.list);
        g_romdatabase.list = search;
        }
}

romdatabase_entry* ini_search_by_md5(md5_byte_t* md5)
{
    if(g_romdatabase.comment==NULL)
        return &empty_entry;
    romdatabase_search* search;
    search = g_romdatabase.md5_lists[md5[0]];

    while (search != NULL && memcmp(search->entry.md5, md5, 16) != 0)
        search = search->next_md5;

    if(search==NULL)
        return &empty_entry;
    else
        return &(search->entry);
}

romdatabase_entry* ini_search_by_crc(unsigned int crc1, unsigned int crc2)
{
    if(g_romdatabase.comment==NULL) 
        return &empty_entry;

    romdatabase_search* search;
    search = g_romdatabase.crc_lists[((crc1 >> 24) & 0xff)];

    while (search != NULL && search->entry.crc1 != crc1 && search->entry.crc2 != crc2)
        search = search->next_crc;

    if(search == NULL) 
        return &empty_entry;
    else
        return &(search->entry);
}


