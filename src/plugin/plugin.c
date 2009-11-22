/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - plugin.c                                                *
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

#include <stdlib.h>

#include "plugin.h"

#include "api/callbacks.h"
#include "api/m64p_common.h"
#include "api/m64p_plugin.h"
#include "api/m64p_types.h"

#include "main/rom.h"
#include "memory/memory.h"

#include "osal/dynamiclib.h"

#include "dummy_audio.h"
#include "dummy_video.h"
#include "dummy_input.h"
#include "dummy_rsp.h"

CONTROL Controls[4];

/* local data structures and functions */
static GFX_INFO gfx_info;
static AUDIO_INFO audio_info;
static CONTROL_INFO control_info;
static RSP_INFO rsp_info;

static int l_RspAttached = 0;
static int l_InputAttached = 0;
static int l_AudioAttached = 0;
static int l_GfxAttached = 0;

static m64p_error plugin_connect_gfx(m64p_handle plugin_handle);
static m64p_error plugin_connect_audio(m64p_handle plugin_handle);
static m64p_error plugin_connect_input(m64p_handle plugin_handle);
static m64p_error plugin_connect_rsp(m64p_handle plugin_handle);

static m64p_error plugin_start_rsp(void);
static m64p_error plugin_start_gfx(void);
static m64p_error plugin_start_audio(void);
static m64p_error plugin_start_input(void);

static unsigned int dummy;

/* global functions */
m64p_error plugin_connect(m64p_plugin_type, m64p_handle plugin_handle);
m64p_error plugin_start(m64p_plugin_type);
m64p_error plugin_check(void);

/* global function pointers */
void (*changeWindow)() = dummyvideo_ChangeWindow;
int  (*initiateGFX)(GFX_INFO Gfx_Info) = dummyvideo_InitiateGFX;
void (*moveScreen)(int x, int y) = dummyvideo_MoveScreen;
void (*processDList)() = dummyvideo_ProcessDList;
void (*processRDPList)() = dummyvideo_ProcessRDPList;
void (*romClosed_gfx)() = dummyvideo_RomClosed;
void (*romOpen_gfx)() = dummyvideo_RomOpen;
void (*showCFB)() = dummyvideo_ShowCFB;
void (*updateScreen)() = dummyvideo_UpdateScreen;
void (*viStatusChanged)() = dummyvideo_ViStatusChanged;
void (*viWidthChanged)() = dummyvideo_ViWidthChanged;
void (*readScreen)(void **dest, int *width, int *height) = dummyvideo_ReadScreen;
void (*setRenderingCallback)(void (*callback)()) = dummyvideo_SetRenderingCallback;

void (*fBRead)(unsigned int addr) = dummyvideo_FBRead;
void (*fBWrite)(unsigned int addr, unsigned int size) = dummyvideo_FBWrite;
void (*fBGetFrameBufferInfo)(void *p) = dummyvideo_FBGetFrameBufferInfo;

void (*aiDacrateChanged)(int SystemType) = dummyaudio_AiDacrateChanged;
void (*aiLenChanged)() = dummyaudio_AiLenChanged;
int  (*initiateAudio)(AUDIO_INFO Audio_Info) = dummyaudio_InitiateAudio;
void (*processAList)() = dummyaudio_ProcessAList;
void (*romOpen_audio)() = dummyaudio_RomOpen;
void (*romClosed_audio)() = dummyaudio_RomClosed;
void (*setSpeedFactor)(int percent) = dummyaudio_SetSpeedFactor;
void (*volumeUp)() = dummyaudio_VolumeUp;
void (*volumeDown)() = dummyaudio_VolumeDown;
int  (*volumeGetLevel)() = dummyaudio_VolumeGetLevel;
void (*volumeSetLevel)(int level) = dummyaudio_VolumeSetLevel;
void (*volumeMute)() = dummyaudio_VolumeMute;
const char * (*volumeGetString)() = dummyaudio_VolumeGetString;

void (*controllerCommand)(int Control, unsigned char *Command) = dummyinput_ControllerCommand;
void (*getKeys)(int Control, BUTTONS *Keys) = dummyinput_GetKeys;
void (*initiateControllers)(CONTROL_INFO ControlInfo) = dummyinput_InitiateControllers;
void (*readController)(int Control, unsigned char *Command) = dummyinput_ReadController;
void (*romClosed_input)() = dummyinput_RomClosed;
void (*romOpen_input)() = dummyinput_RomOpen;
void (*keyDown)(int keymod, int keysym) = dummyinput_SDL_KeyDown;
void (*keyUp)(int keymod, int keysym) = dummyinput_SDL_KeyUp;

unsigned int (*doRspCycles)(unsigned int Cycles) = dummyrsp_DoRspCycles;
void (*initiateRSP)(RSP_INFO Rsp_Info, unsigned int * CycleCount) = dummyrsp_InitiateRSP;
void (*romClosed_RSP)() = dummyrsp_RomClosed;

/* global functions */
m64p_error plugin_connect(m64p_plugin_type type, m64p_handle plugin_handle)
{
    switch(type)
    {
        case M64PLUGIN_GFX:
            if (plugin_handle != NULL && (l_AudioAttached || l_InputAttached || l_RspAttached))
                DebugMessage(M64MSG_WARNING, "Front-end bug: plugins are attached in wrong order.");
            return plugin_connect_gfx(plugin_handle);
        case M64PLUGIN_AUDIO:
            if (plugin_handle != NULL && (l_InputAttached || l_RspAttached))
                DebugMessage(M64MSG_WARNING, "Front-end bug: plugins are attached in wrong order.");
            return plugin_connect_audio(plugin_handle);
        case M64PLUGIN_INPUT:
            if (plugin_handle != NULL && (l_RspAttached))
                DebugMessage(M64MSG_WARNING, "Front-end bug: plugins are attached in wrong order.");
            return plugin_connect_input(plugin_handle);
        case M64PLUGIN_RSP:
            return plugin_connect_rsp(plugin_handle);
        default:
            return M64ERR_INPUT_INVALID;
    }

    return M64ERR_INTERNAL;
}

m64p_error plugin_start(m64p_plugin_type type)
{
    switch(type)
    {
        case M64PLUGIN_RSP:
            return plugin_start_rsp();
        case M64PLUGIN_GFX:
            return plugin_start_gfx();
        case M64PLUGIN_AUDIO:
            return plugin_start_audio();
        case M64PLUGIN_INPUT:
            return plugin_start_input();
        default:
            return M64ERR_INPUT_INVALID;
    }

    return M64ERR_INTERNAL;
}

m64p_error plugin_check(void)
{
    if (!l_GfxAttached)
        DebugMessage(M64MSG_WARNING, "No video plugin attached.  There will be no video output.");
    else if (!l_RspAttached)
        DebugMessage(M64MSG_WARNING, "No RSP plugin attached.  The video output will be corrupted.");
    if (!l_AudioAttached)
        DebugMessage(M64MSG_WARNING, "No audio plugin attached.  There will be no sound output.");
    if (!l_InputAttached)
        DebugMessage(M64MSG_WARNING, "No input plugin attached.  You won't be able to control the game.");

    return M64ERR_SUCCESS;
}

/* local functions */
static void EmptyFunc(void)
{
}

static m64p_error plugin_connect_rsp(m64p_handle plugin_handle)
{
    /* attach the RSP plugin function pointers */
    if (plugin_handle == NULL)
    {
        doRspCycles = dummyrsp_DoRspCycles;
        initiateRSP = dummyrsp_InitiateRSP;
        romClosed_RSP = dummyrsp_RomClosed;
        l_RspAttached = 0;
    }
    else
    {
        ptr_PluginGetVersion getVersion = NULL;
        if (l_RspAttached)
            return M64ERR_INVALID_STATE;
        getVersion = osal_dynlib_getproc(plugin_handle, "PluginGetVersion");
        doRspCycles = osal_dynlib_getproc(plugin_handle, "DoRspCycles");
        initiateRSP = osal_dynlib_getproc(plugin_handle, "InitiateRSP");
        romClosed_RSP = osal_dynlib_getproc(plugin_handle, "RomClosed");
        if (getVersion == NULL || doRspCycles == NULL || initiateRSP == NULL || romClosed_RSP == NULL)
        {
            DebugMessage(M64MSG_ERROR, "broken RSP plugin; function(s) not found.");
            return M64ERR_INPUT_INVALID;
        }
        /* check the version info */
        m64p_plugin_type PluginType;
        int PluginVersion, APIVersion;
        (*getVersion)(&PluginType, &PluginVersion, &APIVersion, NULL, NULL);
        if (PluginType != M64PLUGIN_RSP || PluginVersion < MINIMUM_RSP_VERSION || APIVersion < MINIMUM_RSP_API_VERSION)
        {
            DebugMessage(M64MSG_ERROR, "incompatible RSP plugin");
            return M64ERR_INCOMPATIBLE;
        }
        l_RspAttached = 1;
    }

    return M64ERR_SUCCESS;
}

static m64p_error plugin_connect_input(m64p_handle plugin_handle)
{
    /* attach the Input plugin function pointers */
    if (plugin_handle == NULL)
    {
        controllerCommand = dummyinput_ControllerCommand;
        getKeys = dummyinput_GetKeys;
        initiateControllers = dummyinput_InitiateControllers;
        readController = dummyinput_ReadController;
        romOpen_input = dummyinput_RomOpen;
        romClosed_input = dummyinput_RomClosed;
        keyDown = dummyinput_SDL_KeyDown;
        keyUp = dummyinput_SDL_KeyUp;
        l_InputAttached = 0;
    }
    else
    {
        ptr_PluginGetVersion getVersion = NULL;
        if (l_InputAttached)
            return M64ERR_INVALID_STATE;
        getVersion = osal_dynlib_getproc(plugin_handle, "PluginGetVersion");
        controllerCommand = osal_dynlib_getproc(plugin_handle, "ControllerCommand");
        getKeys = osal_dynlib_getproc(plugin_handle, "GetKeys");
        initiateControllers = osal_dynlib_getproc(plugin_handle, "InitiateControllers");
        readController = osal_dynlib_getproc(plugin_handle, "ReadController");
        romOpen_input = osal_dynlib_getproc(plugin_handle, "RomOpen");
        romClosed_input = osal_dynlib_getproc(plugin_handle, "RomClosed");
        keyDown = osal_dynlib_getproc(plugin_handle, "SDL_KeyDown");
        keyUp = osal_dynlib_getproc(plugin_handle, "SDL_KeyUp");
        if (getVersion == NULL || controllerCommand == NULL || getKeys == NULL || initiateControllers == NULL ||
            readController == NULL || romOpen_input == NULL || romClosed_input == NULL || keyDown == NULL || keyUp == NULL)
        {
            DebugMessage(M64MSG_ERROR, "broken Input plugin; function(s) not found.");
            return M64ERR_INPUT_INVALID;
        }
        /* check the version info */
        m64p_plugin_type PluginType;
        int PluginVersion, APIVersion;
        (*getVersion)(&PluginType, &PluginVersion, &APIVersion, NULL, NULL);
        if (PluginType != M64PLUGIN_INPUT || PluginVersion < MINIMUM_INPUT_VERSION || APIVersion < MINIMUM_INPUT_API_VERSION)
        {
            DebugMessage(M64MSG_ERROR, "incompatible Input plugin");
            return M64ERR_INCOMPATIBLE;
        }
        l_InputAttached = 1;
    }

    return M64ERR_SUCCESS;
}

static m64p_error plugin_connect_audio(m64p_handle plugin_handle)
{
    /* attach the Audio plugin function pointers */
    if (plugin_handle == NULL)
    {
        aiDacrateChanged = dummyaudio_AiDacrateChanged;
        aiLenChanged = dummyaudio_AiLenChanged;
        initiateAudio = dummyaudio_InitiateAudio;
        processAList = dummyaudio_ProcessAList;
        romOpen_audio = dummyaudio_RomOpen;
        romClosed_audio = dummyaudio_RomClosed;
        setSpeedFactor = dummyaudio_SetSpeedFactor;
        volumeUp = dummyaudio_VolumeUp;
        volumeDown = dummyaudio_VolumeDown;
        volumeGetLevel = dummyaudio_VolumeGetLevel;
        volumeSetLevel = dummyaudio_VolumeSetLevel;
        volumeMute = dummyaudio_VolumeMute;
        volumeGetString = dummyaudio_VolumeGetString;
        l_AudioAttached = 0;
    }
    else
    {
        ptr_PluginGetVersion getVersion = NULL;
        if (l_AudioAttached)
            return M64ERR_INVALID_STATE;
        getVersion = osal_dynlib_getproc(plugin_handle, "PluginGetVersion");
        aiDacrateChanged = osal_dynlib_getproc(plugin_handle, "AiDacrateChanged");
        aiLenChanged = osal_dynlib_getproc(plugin_handle, "AiLenChanged");
        initiateAudio = osal_dynlib_getproc(plugin_handle, "InitiateAudio");
        processAList = osal_dynlib_getproc(plugin_handle, "ProcessAList");
        romOpen_audio = osal_dynlib_getproc(plugin_handle, "RomOpen");
        romClosed_audio = osal_dynlib_getproc(plugin_handle, "RomClosed");
        setSpeedFactor = osal_dynlib_getproc(plugin_handle, "SetSpeedFactor");
        volumeUp = osal_dynlib_getproc(plugin_handle, "VolumeUp");
        volumeDown = osal_dynlib_getproc(plugin_handle, "VolumeDown");
        volumeGetLevel = osal_dynlib_getproc(plugin_handle, "VolumeGetLevel");
        volumeSetLevel = osal_dynlib_getproc(plugin_handle, "VolumeSetLevel");
        volumeMute = osal_dynlib_getproc(plugin_handle, "VolumeMute");
        volumeGetString = osal_dynlib_getproc(plugin_handle, "VolumeGetString");
        if (getVersion == NULL || aiDacrateChanged == NULL || aiLenChanged == NULL || initiateAudio == NULL || processAList == NULL ||
            romOpen_audio == NULL || romClosed_audio == NULL || setSpeedFactor == NULL || volumeUp == NULL || volumeDown == NULL ||
            volumeGetLevel == NULL || volumeSetLevel == NULL || volumeMute == NULL || volumeGetString == NULL)
        {
            DebugMessage(M64MSG_ERROR, "broken Audio plugin; function(s) not found.");
            return M64ERR_INPUT_INVALID;
        }
        /* check the version info */
        m64p_plugin_type PluginType;
        int PluginVersion, APIVersion;
        (*getVersion)(&PluginType, &PluginVersion, &APIVersion, NULL, NULL);
        if (PluginType != M64PLUGIN_AUDIO || PluginVersion < MINIMUM_AUDIO_VERSION || APIVersion < MINIMUM_AUDIO_API_VERSION)
        {
            DebugMessage(M64MSG_ERROR, "incompatible Audio plugin");
            return M64ERR_INCOMPATIBLE;
        }
        l_AudioAttached = 1;
    }

    return M64ERR_SUCCESS;
}

static m64p_error plugin_connect_gfx(m64p_handle plugin_handle)
{
    /* attach the Video plugin function pointers */
    if (plugin_handle == NULL)
    {
        changeWindow = dummyvideo_ChangeWindow;
        initiateGFX = dummyvideo_InitiateGFX;
        moveScreen = dummyvideo_MoveScreen;
        processDList = dummyvideo_ProcessDList;
        processRDPList = dummyvideo_ProcessRDPList;
        romClosed_gfx = dummyvideo_RomClosed;
        romOpen_gfx = dummyvideo_RomOpen;
        showCFB = dummyvideo_ShowCFB;
        updateScreen = dummyvideo_UpdateScreen;
        viStatusChanged = dummyvideo_ViStatusChanged;
        viWidthChanged = dummyvideo_ViWidthChanged;
        readScreen = dummyvideo_ReadScreen;
        setRenderingCallback = dummyvideo_SetRenderingCallback;
        fBRead = dummyvideo_FBRead;
        fBWrite = dummyvideo_FBWrite;
        fBGetFrameBufferInfo = dummyvideo_FBGetFrameBufferInfo;
        l_GfxAttached = 0;
    }
    else
    {
        ptr_PluginGetVersion getVersion = NULL;
        if (l_GfxAttached)
            return M64ERR_INVALID_STATE;
        getVersion = osal_dynlib_getproc(plugin_handle, "PluginGetVersion");
        changeWindow = osal_dynlib_getproc(plugin_handle, "ChangeWindow");
        initiateGFX = osal_dynlib_getproc(plugin_handle, "InitiateGFX");
        moveScreen = osal_dynlib_getproc(plugin_handle, "MoveScreen");
        processDList = osal_dynlib_getproc(plugin_handle, "ProcessDList");
        processRDPList = osal_dynlib_getproc(plugin_handle, "ProcessRDPList");
        romClosed_gfx = osal_dynlib_getproc(plugin_handle, "RomClosed");
        romOpen_gfx = osal_dynlib_getproc(plugin_handle, "RomOpen");
        showCFB = osal_dynlib_getproc(plugin_handle, "ShowCFB");
        updateScreen = osal_dynlib_getproc(plugin_handle, "UpdateScreen");
        viStatusChanged = osal_dynlib_getproc(plugin_handle, "ViStatusChanged");
        viWidthChanged = osal_dynlib_getproc(plugin_handle, "ViWidthChanged");
        readScreen = osal_dynlib_getproc(plugin_handle, "ReadScreen");
        setRenderingCallback = osal_dynlib_getproc(plugin_handle, "SetRenderingCallback");
        fBRead = osal_dynlib_getproc(plugin_handle, "FBRead");
        fBWrite = osal_dynlib_getproc(plugin_handle, "FBWrite");
        fBGetFrameBufferInfo = osal_dynlib_getproc(plugin_handle, "FBGetFrameBufferInfo");
        if (getVersion == NULL || changeWindow == NULL || initiateGFX == NULL || moveScreen == NULL || processDList == NULL ||
            processRDPList == NULL || romClosed_gfx == NULL || romOpen_gfx == NULL || showCFB == NULL || updateScreen == NULL ||
            viStatusChanged == NULL || viWidthChanged == NULL || readScreen == NULL || setRenderingCallback == NULL ||
            fBRead == NULL || fBWrite == NULL || fBGetFrameBufferInfo == NULL)
        {
            DebugMessage(M64MSG_ERROR, "broken Video plugin; function(s) not found.");
            return M64ERR_INPUT_INVALID;
        }
        /* check the version info */
        m64p_plugin_type PluginType;
        int PluginVersion, APIVersion;
        (*getVersion)(&PluginType, &PluginVersion, &APIVersion, NULL, NULL);
        if (PluginType != M64PLUGIN_GFX || PluginVersion < MINIMUM_GFX_VERSION || APIVersion < MINIMUM_GFX_API_VERSION)
        {
            DebugMessage(M64MSG_ERROR, "incompatible Video plugin");
            return M64ERR_INCOMPATIBLE;
        }
        l_GfxAttached = 1;
    }

    return M64ERR_SUCCESS;
}

static m64p_error plugin_start_rsp(void)
{
    /* fill in the RSP_INFO data structure */
    rsp_info.RDRAM = (unsigned char *) rdram;
    rsp_info.DMEM = (unsigned char *) SP_DMEM;
    rsp_info.IMEM = (unsigned char *) SP_IMEM;
    rsp_info.MI_INTR_REG = &MI_register.mi_intr_reg;
    rsp_info.SP_MEM_ADDR_REG = &sp_register.sp_mem_addr_reg;
    rsp_info.SP_DRAM_ADDR_REG = &sp_register.sp_dram_addr_reg;
    rsp_info.SP_RD_LEN_REG = &sp_register.sp_rd_len_reg;
    rsp_info.SP_WR_LEN_REG = &sp_register.sp_wr_len_reg;
    rsp_info.SP_STATUS_REG = &sp_register.sp_status_reg;
    rsp_info.SP_DMA_FULL_REG = &sp_register.sp_dma_full_reg;
    rsp_info.SP_DMA_BUSY_REG = &sp_register.sp_dma_busy_reg;
    rsp_info.SP_PC_REG = &rsp_register.rsp_pc;
    rsp_info.SP_SEMAPHORE_REG = &sp_register.sp_semaphore_reg;
    rsp_info.DPC_START_REG = &dpc_register.dpc_start;
    rsp_info.DPC_END_REG = &dpc_register.dpc_end;
    rsp_info.DPC_CURRENT_REG = &dpc_register.dpc_current;
    rsp_info.DPC_STATUS_REG = &dpc_register.dpc_status;
    rsp_info.DPC_CLOCK_REG = &dpc_register.dpc_clock;
    rsp_info.DPC_BUFBUSY_REG = &dpc_register.dpc_bufbusy;
    rsp_info.DPC_PIPEBUSY_REG = &dpc_register.dpc_pipebusy;
    rsp_info.DPC_TMEM_REG = &dpc_register.dpc_tmem;
    rsp_info.CheckInterrupts = EmptyFunc;
    rsp_info.ProcessDlistList = processDList;
    rsp_info.ProcessAlistList = processAList;
    rsp_info.ProcessRdpList = processRDPList;
    rsp_info.ShowCFB = showCFB;

    /* call the RSP plugin  */
    initiateRSP(rsp_info, NULL);

    return M64ERR_SUCCESS;
}

static m64p_error plugin_start_input(void)
{
    int i;

    /* fill in the CONTROL_INFO data structure */
    control_info.Controls = Controls;
    for (i=0; i<4; i++)
      {
         Controls[i].Present = 0;
         Controls[i].RawData = 0;
         Controls[i].Plugin = PLUGIN_NONE;
      }

    /* call the input plugin */
    initiateControllers(control_info);

    return M64ERR_SUCCESS;
}

static m64p_error plugin_start_audio(void)
{
    /* fill in the AUDIO_INFO data structure */
    audio_info.RDRAM = (unsigned char *) rdram;
    audio_info.DMEM = (unsigned char *) SP_DMEM;
    audio_info.IMEM = (unsigned char *) SP_IMEM;
    audio_info.MI_INTR_REG = &(MI_register.mi_intr_reg);
    audio_info.AI_DRAM_ADDR_REG = &(ai_register.ai_dram_addr);
    audio_info.AI_LEN_REG = &(ai_register.ai_len);
    audio_info.AI_CONTROL_REG = &(ai_register.ai_control);
    audio_info.AI_STATUS_REG = &dummy;
    audio_info.AI_DACRATE_REG = &(ai_register.ai_dacrate);
    audio_info.AI_BITRATE_REG = &(ai_register.ai_bitrate);
    audio_info.CheckInterrupts = EmptyFunc;

    /* call the audio plugin */
    if (!initiateAudio(audio_info))
        return M64ERR_PLUGIN_FAIL;

    return M64ERR_SUCCESS;
}

static m64p_error plugin_start_gfx(void)
{
    /* fill in the GFX_INFO data structure */
    gfx_info.HEADER = (unsigned char *) rom;
    gfx_info.RDRAM = (unsigned char *) rdram;
    gfx_info.DMEM = (unsigned char *) SP_DMEM;
    gfx_info.IMEM = (unsigned char *) SP_IMEM;
    gfx_info.MI_INTR_REG = &(MI_register.mi_intr_reg);
    gfx_info.DPC_START_REG = &(dpc_register.dpc_start);
    gfx_info.DPC_END_REG = &(dpc_register.dpc_end);
    gfx_info.DPC_CURRENT_REG = &(dpc_register.dpc_current);
    gfx_info.DPC_STATUS_REG = &(dpc_register.dpc_status);
    gfx_info.DPC_CLOCK_REG = &(dpc_register.dpc_clock);
    gfx_info.DPC_BUFBUSY_REG = &(dpc_register.dpc_bufbusy);
    gfx_info.DPC_PIPEBUSY_REG = &(dpc_register.dpc_pipebusy);
    gfx_info.DPC_TMEM_REG = &(dpc_register.dpc_tmem);
    gfx_info.VI_STATUS_REG = &(vi_register.vi_status);
    gfx_info.VI_ORIGIN_REG = &(vi_register.vi_origin);
    gfx_info.VI_WIDTH_REG = &(vi_register.vi_width);
    gfx_info.VI_INTR_REG = &(vi_register.vi_v_intr);
    gfx_info.VI_V_CURRENT_LINE_REG = &(vi_register.vi_current);
    gfx_info.VI_TIMING_REG = &(vi_register.vi_burst);
    gfx_info.VI_V_SYNC_REG = &(vi_register.vi_v_sync);
    gfx_info.VI_H_SYNC_REG = &(vi_register.vi_h_sync);
    gfx_info.VI_LEAP_REG = &(vi_register.vi_leap);
    gfx_info.VI_H_START_REG = &(vi_register.vi_h_start);
    gfx_info.VI_V_START_REG = &(vi_register.vi_v_start);
    gfx_info.VI_V_BURST_REG = &(vi_register.vi_v_burst);
    gfx_info.VI_X_SCALE_REG = &(vi_register.vi_x_scale);
    gfx_info.VI_Y_SCALE_REG = &(vi_register.vi_y_scale);
    gfx_info.CheckInterrupts = EmptyFunc;

    /* call the audio plugin */
    if (!initiateGFX(gfx_info))
        return M64ERR_PLUGIN_FAIL;

    return M64ERR_SUCCESS;
}


