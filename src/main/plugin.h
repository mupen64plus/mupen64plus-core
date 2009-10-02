/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - plugin.h                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#ifndef PLUGIN_H
#define PLUGIN_H

#include "winlnxdefs.h"
#include "util.h" // list_t type

//--------------------- plugin storage type ----------------
typedef struct _plugin
{
   char *file_name;
   char *plugin_name;
   void *handle;
   int type;
} plugin;

extern list_t g_PluginList;

int   plugin_scan_file(const char *filepath, WORD PluginType);
void  plugin_scan_directory(const char *plugindir);
void  plugin_set_dirs(char* configdir, char* installdir);
void  plugin_load_plugins(const char *gfx_name, 
              const char *audio_name, 
              const char *input_name,
              const char *RSP_name);
void plugin_close_plugins();
void  plugin_delete_list();
plugin *plugin_get_by_name(const char *name);
char *plugin_filename_by_name(const char *name);
char *plugin_name_by_filename(const char *filename);

void  plugin_exec_config_with_wid(const char *name, HWND wid);
void  plugin_exec_test_with_wid(const char *name, HWND wid);
void  plugin_exec_about_with_wid(const char *name, HWND wid);

void  plugin_exec_config(const char *name);
void  plugin_exec_test(const char *name);
void  plugin_exec_about(const char *name);

void plugin_load_gfx_plugin(const char* gfx_name);
void plugin_load_audio_plugin(const char* audio_name);
void plugin_load_input_plugin(const char* input_name);
void plugin_load_rsp_plugin(const char* RSP_name);

/* Plugin types */
#define PLUGIN_TYPE_RSP         1
#define PLUGIN_TYPE_GFX         2
#define PLUGIN_TYPE_AUDIO               3
#define PLUGIN_TYPE_CONTROLLER          4

/*** Controller plugin's ****/
#define PLUGIN_NONE                             1
#define PLUGIN_MEMPAK                           2
#define PLUGIN_RUMBLE_PAK           3 // not implemeted for non raw data
#define PLUGIN_TANSFER_PAK          4 // not implemeted for non raw data
#define PLUGIN_RAW              5 // the controller plugin is passed in raw data

/*** Audio plugin system types ***/
#define SYSTEM_NTSC                 0
#define SYSTEM_PAL                  1
#define SYSTEM_MPAL                 2

/***** Structures *****/
typedef struct {
    WORD Version;
    WORD Type;
    char Name[100];       /* Name of the DLL */

    /* If DLL supports memory these memory options then set them to TRUE or FALSE
       if it does not support it */
    BOOL NormalMemory;    /* a normal BYTE array */ 
    BOOL MemoryBswaped;   /* a normal BYTE array where the memory has been pre
                             bswap on a dword (32 bits) boundry */
} PLUGIN_INFO;

typedef struct {
    HINSTANCE hInst;
    BOOL MemoryBswaped;    /* If this is set to TRUE, then the memory has been pre
                              bswap on a dword (32 bits) boundry */
    BYTE * RDRAM;
    BYTE * DMEM;
    BYTE * IMEM;

    DWORD * MI_INTR_REG;

    DWORD * SP_MEM_ADDR_REG;
    DWORD * SP_DRAM_ADDR_REG;
    DWORD * SP_RD_LEN_REG;
    DWORD * SP_WR_LEN_REG;
    DWORD * SP_STATUS_REG;
    DWORD * SP_DMA_FULL_REG;
    DWORD * SP_DMA_BUSY_REG;
    DWORD * SP_PC_REG;
    DWORD * SP_SEMAPHORE_REG;

    DWORD * DPC_START_REG;
    DWORD * DPC_END_REG;
    DWORD * DPC_CURRENT_REG;
    DWORD * DPC_STATUS_REG;
    DWORD * DPC_CLOCK_REG;
    DWORD * DPC_BUFBUSY_REG;
    DWORD * DPC_PIPEBUSY_REG;
    DWORD * DPC_TMEM_REG;

    void (*CheckInterrupts)( void );
    void (*ProcessDlistList)( void );
    void (*ProcessAlistList)( void );
    void (*ProcessRdpList)( void );
    void (*ShowCFB)( void );
} RSP_INFO;

typedef struct {
    HWND hWnd;         /* Render window */
    HWND hStatusBar;       /* if render window does not have a status bar then this is NULL */

    BOOL MemoryBswaped;    // If this is set to TRUE, then the memory has been pre
                           //   bswap on a dword (32 bits) boundry 
                           //   eg. the first 8 bytes are stored like this:
                           //        4 3 2 1   8 7 6 5

    BYTE * HEADER;         // This is the rom header (first 40h bytes of the rom
                   // This will be in the same memory format as the rest of the memory.
    BYTE * RDRAM;
    BYTE * DMEM;
    BYTE * IMEM;

    DWORD * MI_INTR_REG;

    DWORD * DPC_START_REG;
    DWORD * DPC_END_REG;
    DWORD * DPC_CURRENT_REG;
    DWORD * DPC_STATUS_REG;
    DWORD * DPC_CLOCK_REG;
    DWORD * DPC_BUFBUSY_REG;
    DWORD * DPC_PIPEBUSY_REG;
    DWORD * DPC_TMEM_REG;

    DWORD * VI_STATUS_REG;
    DWORD * VI_ORIGIN_REG;
    DWORD * VI_WIDTH_REG;
    DWORD * VI_INTR_REG;
    DWORD * VI_V_CURRENT_LINE_REG;
    DWORD * VI_TIMING_REG;
    DWORD * VI_V_SYNC_REG;
    DWORD * VI_H_SYNC_REG;
    DWORD * VI_LEAP_REG;
    DWORD * VI_H_START_REG;
    DWORD * VI_V_START_REG;
    DWORD * VI_V_BURST_REG;
    DWORD * VI_X_SCALE_REG;
    DWORD * VI_Y_SCALE_REG;

    void (*CheckInterrupts)( void );
} GFX_INFO;

typedef struct {
    HWND hwnd;
    HINSTANCE hinst;

    BOOL MemoryBswaped;    // If this is set to TRUE, then the memory has been pre
                           //   bswap on a dword (32 bits) boundry 
                           //   eg. the first 8 bytes are stored like this:
                           //        4 3 2 1   8 7 6 5
    BYTE * HEADER;  // This is the rom header (first 40h bytes of the rom
                    // This will be in the same memory format as the rest of the memory.
    BYTE * RDRAM;
    BYTE * DMEM;
    BYTE * IMEM;

    DWORD * MI_INTR_REG;

    DWORD * AI_DRAM_ADDR_REG;
    DWORD * AI_LEN_REG;
    DWORD * AI_CONTROL_REG;
    DWORD * AI_STATUS_REG;
    DWORD * AI_DACRATE_REG;
    DWORD * AI_BITRATE_REG;

    void (*CheckInterrupts)( void );
} AUDIO_INFO;

typedef struct {
    BOOL Present;
    BOOL RawData;
    int  Plugin;
} CONTROL;

typedef union {
    DWORD Value;
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
    HWND hMainWindow;
    HINSTANCE hinst;

    BOOL MemoryBswaped;     // If this is set to TRUE, then the memory has been pre
                            //   bswap on a dword (32 bits) boundry, only effects header. 
                            //  eg. the first 8 bytes are stored like this:
                            //        4 3 2 1   8 7 6 5
    BYTE * HEADER;          // This is the rom header (first 40h bytes of the rom)
    CONTROL *Controls;      // A pointer to an array of 4 controllers .. eg:
                            // CONTROL Controls[4];
} CONTROL_INFO;

extern CONTROL Controls[4];

extern void (*getDllInfo)(PLUGIN_INFO *PluginInfo);
extern void (*dllConfig)(HWND hParent);
extern void (*dllTest)(HWND hParent);
extern void (*dllAbout)(HWND hParent);

extern void (*changeWindow)();
extern void (*closeDLL_gfx)();
extern BOOL (*initiateGFX)(GFX_INFO Gfx_Info);
extern void (*processDList)();
extern void (*processRDPList)();
extern void (*romClosed_gfx)();
extern void (*romOpen_gfx)();
extern void (*showCFB)();
extern void (*updateScreen)();
extern void (*viStatusChanged)();
extern void (*viWidthChanged)();
extern void (*readScreen)(void **dest, int *width, int *height);
extern void (*captureScreen)(char *dirpath);
extern void (*setRenderingCallback)(void (*callback)());
extern void (*moveScreen)(int x, int y);

extern void (*aiDacrateChanged)(int SystemType);
extern void (*aiLenChanged)();
extern DWORD (*aiReadLength)();
//extern void (*aiUpdate)(BOOL Wait);
extern void (*closeDLL_audio)();
extern BOOL (*initiateAudio)(AUDIO_INFO Audio_Info);
extern void (*processAList)();
extern void (*romClosed_audio)();
extern void (*romOpen_audio)();
extern void (*setSpeedFactor)(int percent);
extern void (*volumeUp)();
extern void (*volumeDown)();
extern void (*volumeMute)();
extern const char * (*volumeGetString)();

extern void (*closeDLL_input)();
extern void (*controllerCommand)(int Control, BYTE * Command);
extern void (*getKeys)(int Control, BUTTONS *Keys);
extern void (*old_initiateControllers)(HWND hMainWindow, CONTROL Controls[4]);
extern void (*initiateControllers)(CONTROL_INFO ControlInfo);
extern void (*readController)(int Control, BYTE *Command);
extern void (*romClosed_input)();
extern void (*romOpen_input)();
extern void (*keyDown)(WPARAM wParam, LPARAM lParam);
extern void (*keyUp)(WPARAM wParam, LPARAM lParam);
extern void (*setConfigDir)(char *configDir);

extern void (*closeDLL_RSP)();
extern DWORD (*doRspCycles)(DWORD Cycles);
extern void (*initiateRSP)(RSP_INFO Rsp_Info, DWORD * CycleCount);
extern void (*romClosed_RSP)();

// frame buffer plugin spec extension

typedef struct
{
   DWORD addr;
   DWORD size;
   DWORD width;
   DWORD height;
} FrameBufferInfo;

extern void (*fBRead)(DWORD addr);
extern void (*fBWrite)(DWORD addr, DWORD size);
extern void (*fBGetFrameBufferInfo)(void *p);

extern HINSTANCE g_ProgramInstance;
extern HWND g_RenderWindow;
extern HWND g_StatusBar;

#endif

