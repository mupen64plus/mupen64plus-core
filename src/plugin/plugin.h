/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - plugin.h                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *   Copyright (C) 2009 Richard Goedeken                                   *
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

#ifndef PLUGIN_H
#define PLUGIN_H

#include "api/m64p_types.h"

extern m64p_error plugin_connect(m64p_plugin_type, m64p_handle plugin_handle);
extern m64p_error plugin_start(m64p_plugin_type);

/*** Controller plugin's ****/
#define PLUGIN_NONE                             1
#define PLUGIN_MEMPAK                           2
#define PLUGIN_RUMBLE_PAK                       3 /* not implemeted for non raw data */
#define PLUGIN_TANSFER_PAK                      4 /* not implemeted for non raw data */
#define PLUGIN_RAW                              5 /* the controller plugin is passed in raw data */

/*** Audio plugin system types ***/
#define SYSTEM_NTSC                 0
#define SYSTEM_PAL                  1
#define SYSTEM_MPAL                 2

/*** Version requirement information ***/
#define MINIMUM_RSP_VERSION   0x20000
#define MINIMUM_GFX_VERSION   0x20000
#define MINIMUM_AUDIO_VERSION 0x20000
#define MINIMUM_INPUT_VERSION 0x20000

#define MINIMUM_RSP_API_VERSION   0x10000
#define MINIMUM_GFX_API_VERSION   0x10000
#define MINIMUM_AUDIO_API_VERSION 0x10000
#define MINIMUM_INPUT_API_VERSION 0x10000

/***** Structures *****/
typedef struct {
    int MemoryBswaped;    /* If this is set to TRUE, then the memory has been pre
                              bswap on a unsigned int (32 bits) boundry */
    unsigned char * RDRAM;
    unsigned char * DMEM;
    unsigned char * IMEM;

    unsigned int * MI_INTR_REG;

    unsigned int * SP_MEM_ADDR_REG;
    unsigned int * SP_DRAM_ADDR_REG;
    unsigned int * SP_RD_LEN_REG;
    unsigned int * SP_WR_LEN_REG;
    unsigned int * SP_STATUS_REG;
    unsigned int * SP_DMA_FULL_REG;
    unsigned int * SP_DMA_BUSY_REG;
    unsigned int * SP_PC_REG;
    unsigned int * SP_SEMAPHORE_REG;

    unsigned int * DPC_START_REG;
    unsigned int * DPC_END_REG;
    unsigned int * DPC_CURRENT_REG;
    unsigned int * DPC_STATUS_REG;
    unsigned int * DPC_CLOCK_REG;
    unsigned int * DPC_BUFBUSY_REG;
    unsigned int * DPC_PIPEBUSY_REG;
    unsigned int * DPC_TMEM_REG;

    void (*CheckInterrupts)( void );
    void (*ProcessDlistList)( void );
    void (*ProcessAlistList)( void );
    void (*ProcessRdpList)( void );
    void (*ShowCFB)( void );
} RSP_INFO;

typedef struct {
    int MemoryBswaped;    /* If this is set to TRUE, then the memory has been pre
                             bswap on a unsigned int (32 bits) boundry 
                             eg. the first 8 unsigned chars are stored like this:
                                  4 3 2 1   8 7 6 5 */

    unsigned char * HEADER;         /* This is the rom header (first 40h unsigned chars of the rom
                                       This will be in the same memory format as the rest of the memory.*/
    unsigned char * RDRAM;
    unsigned char * DMEM;
    unsigned char * IMEM;

    unsigned int * MI_INTR_REG;

    unsigned int * DPC_START_REG;
    unsigned int * DPC_END_REG;
    unsigned int * DPC_CURRENT_REG;
    unsigned int * DPC_STATUS_REG;
    unsigned int * DPC_CLOCK_REG;
    unsigned int * DPC_BUFBUSY_REG;
    unsigned int * DPC_PIPEBUSY_REG;
    unsigned int * DPC_TMEM_REG;

    unsigned int * VI_STATUS_REG;
    unsigned int * VI_ORIGIN_REG;
    unsigned int * VI_WIDTH_REG;
    unsigned int * VI_INTR_REG;
    unsigned int * VI_V_CURRENT_LINE_REG;
    unsigned int * VI_TIMING_REG;
    unsigned int * VI_V_SYNC_REG;
    unsigned int * VI_H_SYNC_REG;
    unsigned int * VI_LEAP_REG;
    unsigned int * VI_H_START_REG;
    unsigned int * VI_V_START_REG;
    unsigned int * VI_V_BURST_REG;
    unsigned int * VI_X_SCALE_REG;
    unsigned int * VI_Y_SCALE_REG;

    void (*CheckInterrupts)( void );
} GFX_INFO;

typedef struct {
    int MemoryBswaped;    /* If this is set to TRUE, then the memory has been pre
                             bswap on a unsigned int (32 bits) boundry 
                             eg. the first 8 unsigned chars are stored like this:
                                  4 3 2 1   8 7 6 5 */

    unsigned char * HEADER;         /* This is the rom header (first 40h unsigned chars of the rom
                                       This will be in the same memory format as the rest of the memory.*/
    unsigned char * RDRAM;
    unsigned char * DMEM;
    unsigned char * IMEM;

    unsigned int * MI_INTR_REG;

    unsigned int * AI_DRAM_ADDR_REG;
    unsigned int * AI_LEN_REG;
    unsigned int * AI_CONTROL_REG;
    unsigned int * AI_STATUS_REG;
    unsigned int * AI_DACRATE_REG;
    unsigned int * AI_BITRATE_REG;

    void (*CheckInterrupts)( void );
} AUDIO_INFO;

typedef struct {
    int Present;
    int RawData;
    int  Plugin;
} CONTROL;

typedef union {
    unsigned int Value;
    struct {
        unsigned R_DPAD       : 1;
        unsigned L_DPAD       : 1;
        unsigned D_DPAD       : 1;
        unsigned U_DPAD       : 1;
        unsigned START_BUTTON : 1;
        unsigned Z_TRIG       : 1;
        unsigned B_BUTTON     : 1;
        unsigned A_BUTTON     : 1;

        unsigned R_CBUTTON    : 1;
        unsigned L_CBUTTON    : 1;
        unsigned D_CBUTTON    : 1;
        unsigned U_CBUTTON    : 1;
        unsigned R_TRIG       : 1;
        unsigned L_TRIG       : 1;
        unsigned Reserved1    : 1;
        unsigned Reserved2    : 1;

        signed   Y_AXIS       : 8;

        signed   X_AXIS       : 8;
    };
} BUTTONS;

typedef struct {
    int MemoryBswaped;    /* If this is set to TRUE, then the memory has been pre
                             bswap on a unsigned int (32 bits) boundry 
                             eg. the first 8 unsigned chars are stored like this:
                                  4 3 2 1   8 7 6 5 */

    unsigned char * HEADER;         /* This is the rom header (first 40h unsigned chars of the rom
                                       This will be in the same memory format as the rest of the memory.*/
    CONTROL *Controls;      /* A pointer to an array of 4 controllers .. eg:
                               CONTROL Controls[4]; */
} CONTROL_INFO;

extern CONTROL Controls[4];

/* video plugin function pointers */
extern void (*changeWindow)();
extern int  (*initiateGFX)(GFX_INFO Gfx_Info);
extern void (*moveScreen)(int x, int y);
extern void (*processDList)();
extern void (*processRDPList)();
extern void (*romClosed_gfx)();
extern void (*romOpen_gfx)();
extern void (*showCFB)();
extern void (*updateScreen)();
extern void (*viStatusChanged)();
extern void (*viWidthChanged)();
extern void (*readScreen)(void **dest, int *width, int *height);
extern void (*setRenderingCallback)(void (*callback)());
/* frame buffer plugin spec extension */
typedef struct
{
   unsigned int addr;
   unsigned int size;
   unsigned int width;
   unsigned int height;
} FrameBufferInfo;
extern void (*fBRead)(unsigned int addr);
extern void (*fBWrite)(unsigned int addr, unsigned int size);
extern void (*fBGetFrameBufferInfo)(void *p);

/* audio plugin function pointers */
extern void (*aiDacrateChanged)(int SystemType);
extern void (*aiLenChanged)();
extern int  (*initiateAudio)(AUDIO_INFO Audio_Info);
extern void (*processAList)();
extern void (*romClosed_audio)();
extern void (*romOpen_audio)();
extern void (*setSpeedFactor)(int percent);
extern void (*volumeUp)();
extern void (*volumeDown)();
extern int  (*volumeGetLevel)();
extern void (*volumeSetLevel)(int level);
extern void (*volumeMute)();
extern const char * (*volumeGetString)();

/* input plugin function pointers */
extern void (*controllerCommand)(int Control, unsigned char *Command);
extern void (*getKeys)(int Control, BUTTONS *Keys);
extern void (*initiateControllers)(CONTROL_INFO ControlInfo);
extern void (*readController)(int Control, unsigned char *Command);
extern void (*romClosed_input)();
extern void (*romOpen_input)();
extern void (*keyDown)(int keymod, int keysym);
extern void (*keyUp)(int keymod, int keysym);

/* RSP plugin function pointers */
extern unsigned int (*doRspCycles)(unsigned int Cycles);
extern void (*initiateRSP)(RSP_INFO Rsp_Info, unsigned int *CycleCount);
extern void (*romClosed_RSP)();

#endif

