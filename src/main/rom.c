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
#include "romcache.h"
#include "translate.h"
#include "main.h"
#include "util.h"

#include "../memory/memory.h"
#include "../opengl/osd.h"

#define CHUNKSIZE 1024*128 /* Read files 128KB at a time. */

#ifndef NO_GUI
#include "gui.h"
#endif

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

/* Open a file and test if its a zip or 7zip archive. If so load check if *archivefile
 * is a rom and load *loadlength into the returned pointer. If *archivefile is not a rom
 * in a many file archive, function will keep checking until a rom is found or the 
 * end of the archive is reached. Returns NULL if unable to find a rom. The large function
 * API is to allow for persistent 7zip memory space.
 */
unsigned char* load_archive_rom(const char* filename, int* romsize, unsigned char* compressiontype, int* loadlength, unsigned int* archivefile, UInt32* blockIndex, Byte** outBuffer, size_t* outBufferSize, CFileInStream* archiveStream, CArchiveDatabaseEx* db)
{
    int status;
    unsigned int filecounter = 0;
    unsigned short romread = 0;
    unsigned char* localrom;

    /* Zip roms. */
    unsigned char buffer[4];
    unzFile zipromfile;
    unz_file_info fileinfo;
    char szFileName[256], szExtraField[256], szComment[256];
    zipromfile = unzOpen(filename);
    if(zipromfile!=NULL) 
        {
        if(unzGoToFirstFile(zipromfile)==UNZ_OK)
            {
            /* Get first valid rom in archive. */
            do
                {
                if(unzGetCurrentFileInfo(zipromfile, &fileinfo, szFileName, 255, 
                szExtraField, 255, szComment, 255)!=UNZ_OK)
                    break;
                if(unzOpenCurrentFile(zipromfile)!=UNZ_OK)
                    break;

                if(fileinfo.uncompressed_size>=4&&filecounter>=*archivefile)
                    {
                    if(unzReadCurrentFile(zipromfile, buffer, 4)!=4)
                        break;
                    if(is_valid_rom(buffer))
                        {
                        unzOpenCurrentFile(zipromfile);
                        *romsize = fileinfo.uncompressed_size;
                        localrom = (unsigned char*)malloc(*loadlength*sizeof(unsigned char));
                        if(localrom==NULL)
                            {
                            fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                            return NULL;
                            }

                        if(unzReadCurrentFile(zipromfile, localrom, *loadlength)==*loadlength)
                            {
                            *compressiontype = ZIP_COMPRESSION;
                            unzCloseCurrentFile(zipromfile);
                            romread = 1;
                            break;
                            }
                        else
                            free(localrom);
                        }
                    }
                ++filecounter;
                }
            while ((status=unzGoToNextFile(zipromfile)) != UNZ_END_OF_LIST_OF_FILE);

            unzClose(zipromfile);
            }
        }
    else
        {
        /* 7zip */
        ISzAlloc allocImp;
        allocImp.Free = SzFree;
        allocImp.Alloc = SzAlloc;

        ISzAlloc allocTempImp;
        allocTempImp.Alloc = SzAllocTemp;
        allocTempImp.Free = SzFreeTemp;

        if(archiveStream->File==NULL)
            {
            archiveStream->File = fopen(filename, "rb");
            if(archiveStream->File==NULL)
                return NULL;
            status = SzArchiveOpen(&(archiveStream->InStream), db, &allocImp, &allocTempImp);
            if(status==SZ_OK)
                printf("Deflating 7zip archive.\n");
            }
        else
            status = SZ_OK;

        if(status==SZ_OK)
            {
            for (filecounter = *archivefile; filecounter < db->Database.NumFiles; ++filecounter)
                {
                size_t offset;
                size_t outSizeProcessed;

                CFileItem *f = db->Database.Files + filecounter;
                status = SzExtract(&(archiveStream->InStream), db, filecounter, blockIndex, outBuffer, outBufferSize, &offset, &outSizeProcessed, &allocImp, &allocTempImp);
                if(status!=SZ_OK)
                    {
                    if(status==SZE_NOTIMPL)
                        printf("7zip decoder doesn't support this archive.");
                    else if(status==SZE_OUTOFMEMORY)
                        printf("7zip decoder can not allocate memory\n");
                    else if(status==SZE_CRC_ERROR)
                       printf("7zip CRC error");
                    else
                       printf("7zip Error# %d\n", status);
                    break; 
                    }
                if(is_valid_rom(*outBuffer+offset))
                    {
                    *romsize=f->Size;
                    *compressiontype = SZIP_COMPRESSION;
                    localrom = (unsigned char*)malloc(*loadlength*sizeof(unsigned char));
                    if(localrom==NULL)
                        {
                        fprintf(stderr, "%s, %d: Out of memory!\n", __FILE__, __LINE__);
                        return NULL;
                        }
                    memcpy(localrom,*outBuffer+offset,*loadlength);
                    romread = 1;
                    break;
                    }
                }
            }
        }

    /* File invalid, or valid rom not found in file. */
    if(romread==0)
        return NULL;

    *archivefile = filecounter;
    return localrom;
}

static int ask_bad(void)
{
#ifndef NO_GUI
    if(!g_Noask)
        return gui_message(GUI_MESSAGE_CONFIRM,
                           tr("The rom you are trying to load is probably a bad dump!\n"
                              "Be warned that this will probably give unexpected results.\n"
                              "Do you still want to run it?"));
    else
#endif
        printf(tr("The rom you are trying to load is probably a bad dump!\n"
                  "Be warned that this will probably give unexpected results.\n"));

    return 1;
}

static int ask_hack(void)
{
#ifndef NO_GUI
    if(!g_Noask)
        return gui_message(GUI_MESSAGE_CONFIRM, tr("The rom you are trying to load is probably a hack!\n"
                                 "Be warned that this will probably give unexpected results.\n"
                                 "Do you still want to run it?"));
    else
#endif
        printf(tr("The rom you are trying to load is probably a hack!\n"
                  "Be warned that this will probably give unexpected results.\n"));

    return 1;
}

int open_rom(const char* filename, unsigned int archivefile)
{
    if(g_EmulatorRunning)
         {
#ifndef NO_GUI
         if(!g_Noask)
             {
             if(!gui_message(GUI_MESSAGE_CONFIRM,
                             tr("Emulation is running. Do you want to\n"
                                "stop it and load the selected rom?")))
                 return -1;
             }
#endif
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

    CFileInStream archiveStream;
    archiveStream.File = NULL;
    archiveStream.InStream.Read = SzFileReadImp;
    archiveStream.InStream.Seek = SzFileSeekImp;

    CArchiveDatabaseEx db;
    SzArDbExInit(&db);
    CrcGenerateTable();

    strncpy(buffer, filename, PATH_MAX-1);
    buffer[PATH_MAX-1] = 0;
    if ((rom=load_single_rom(filename, &taille_rom, &compressiontype, &taille_rom))==NULL)
        {
        if((rom=load_archive_rom(filename, &taille_rom, &compressiontype, &taille_rom, &archivefile, &blockIndex, &outBuffer, &outBufferSize, &archiveStream, &db))==NULL)
            {
            error_message(tr("Couldn't load Rom!")); 
            return -1;
            }
        }

    if(outBuffer)
        free(outBuffer);
    if(archiveStream.File)
        fclose(archiveStream.File);
    SzArDbExFree(&db, free);

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

