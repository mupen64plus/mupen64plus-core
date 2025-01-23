/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/vidext.c                                       *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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
                       
/* This file contains the Core video extension functions which will be exported
 * outside of the core library.
 */

#include <SDL.h>
/* we need at least SDL 2.0.6 for vulkan */
#if !SDL_VERSION_ATLEAST(2,0,6)
    #undef VIDEXT_VULKAN
#endif
#ifdef VIDEXT_VULKAN
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#endif
#include <stdlib.h>
#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "osal/preproc.h"
#include "../osd/osd.h"
#include "callbacks.h"
#include "m64p_types.h"
#include "m64p_vidext.h"
#include "vidext.h"

#ifndef USE_GLES
static int l_ForceCompatibilityContext = 1;
#endif

/* local variables */
static m64p_video_extension_functions l_ExternalVideoFuncTable = {17, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static int l_VideoExtensionActive = 0;
static int l_VideoOutputActive = 0;
static int l_Fullscreen = 0;
static int l_SwapControl = 0;
static m64p_render_mode l_RenderMode = M64P_RENDER_OPENGL;
static SDL_Window *l_pWindow = NULL;
static SDL_GLContext *l_pGLContext;
static char* l_pWindowTitle = NULL;
#ifdef VIDEXT_VULKAN
static const char** l_VulkanExtensionNames = NULL;
#endif

/* local helper functions */

static int get_current_display(void)
{
    const char *variable = SDL_getenv("SDL_VIDEO_FULLSCREEN_DISPLAY");
    if ( !variable ) {
        variable = SDL_getenv("SDL_VIDEO_FULLSCREEN_HEAD");
    }
    if ( variable ) {
        return SDL_atoi(variable);
    } else {
        return 0;
    }
}

/* global function for use by frontend.c */
m64p_error OverrideVideoFunctions(m64p_video_extension_functions *VideoFunctionStruct)
{
    /* check input data */
    if (VideoFunctionStruct == NULL)
        return M64ERR_INPUT_ASSERT;
    if (VideoFunctionStruct->Functions < 17)
        return M64ERR_INPUT_INVALID;

    /* disable video extension if any of the function pointers are NULL */
    if (VideoFunctionStruct->VidExtFuncInit == NULL ||
        VideoFunctionStruct->VidExtFuncInitWithRenderMode == NULL ||
        VideoFunctionStruct->VidExtFuncQuit == NULL ||
        VideoFunctionStruct->VidExtFuncListModes == NULL ||
        VideoFunctionStruct->VidExtFuncListRates == NULL ||
        VideoFunctionStruct->VidExtFuncSetMode == NULL ||
        VideoFunctionStruct->VidExtFuncSetModeWithRate == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetProc == NULL ||
        VideoFunctionStruct->VidExtFuncGLSetAttr == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetAttr == NULL ||
        VideoFunctionStruct->VidExtFuncGLSwapBuf == NULL ||
        VideoFunctionStruct->VidExtFuncSetCaption == NULL ||
        VideoFunctionStruct->VidExtFuncToggleFS == NULL ||
        VideoFunctionStruct->VidExtFuncResizeWindow == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetDefaultFramebuffer == NULL ||
        VideoFunctionStruct->VidExtFuncVKGetSurface == NULL ||
        VideoFunctionStruct->VidExtFuncVKGetInstanceExtensions == NULL)
    {
        l_ExternalVideoFuncTable.Functions = 17;
        memset(&l_ExternalVideoFuncTable.VidExtFuncInit, 0, 17 * sizeof(void *));
        l_VideoExtensionActive = 0;
        return M64ERR_SUCCESS;
    }

    /* otherwise copy in the override function pointers */
    memcpy(&l_ExternalVideoFuncTable, VideoFunctionStruct, sizeof(m64p_video_extension_functions));
    l_VideoExtensionActive = 1;
    return M64ERR_SUCCESS;
}

int VidExt_InFullscreenMode(void)
{
    return l_Fullscreen;
}

int VidExt_VideoRunning(void)
{
    return l_VideoOutputActive;
}

/* video extension functions to be called by the video plugin */
EXPORT m64p_error CALL VidExt_Init(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncInit)();

    /* redirect to VidExt_InitWithRenderMode with OpenGL render mode */
    return VidExt_InitWithRenderMode(M64P_RENDER_OPENGL);
}

EXPORT m64p_error CALL VidExt_InitWithRenderMode(m64p_render_mode RenderMode)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncInitWithRenderMode)(RenderMode);

    /* set global render mode */
#ifdef VIDEXT_VULKAN
    l_RenderMode = RenderMode;
#endif

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    /* retrieve default swap interval/VSync */
    if (RenderMode == M64P_RENDER_OPENGL) {
        l_SwapControl = SDL_GL_GetSwapInterval();
    }

#if SDL_VERSION_ATLEAST(2,24,0)
    /* fix DPI scaling issues on Windows */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
    {
        DebugMessage(M64MSG_ERROR, "SDL video subsystem init failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    /* attempt to load vulkan library  */
    if (RenderMode == M64P_RENDER_VULKAN)
    {
#ifdef VIDEXT_VULKAN
        if (SDL_Vulkan_LoadLibrary(NULL) == -1)
        {
            DebugMessage(M64MSG_ERROR, "SDL_Vulkan_LoadLibrary failed: %s", SDL_GetError());
            return M64ERR_SYSTEM_FAIL;
        }
#else
        return M64ERR_UNSUPPORTED;
#endif
    }

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_Quit(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncQuit)();
        if (rval == M64ERR_SUCCESS)
        {
            l_VideoOutputActive = 0;
            StateChanged(M64CORE_VIDEO_MODE, M64VIDEO_NONE);
        }
        return rval;
    }

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    SDL_ShowCursor(SDL_ENABLE);

    if (l_pGLContext != NULL) {
        SDL_GL_DeleteContext(l_pGLContext);
        l_pGLContext = NULL;
    }
    if (l_pWindow != NULL) {
        SDL_DestroyWindow(l_pWindow);
        l_pWindow = NULL;
    }
    if (l_pWindowTitle != NULL) {
        free(l_pWindowTitle);
        l_pWindowTitle = NULL;
    }

#ifdef VIDEXT_VULKAN
    if (l_RenderMode == M64P_RENDER_VULKAN) {
        SDL_Vulkan_UnloadLibrary();
    }
    if (l_VulkanExtensionNames != NULL) {
        free(l_VulkanExtensionNames);
        l_VulkanExtensionNames = NULL;
    }
#endif
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    l_VideoOutputActive = 0;
    StateChanged(M64CORE_VIDEO_MODE, M64VIDEO_NONE);

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ListFullscreenModes(m64p_2d_size *SizeArray, int *NumSizes)
{
    SDL_DisplayMode mode;
    int count;
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncListModes)(SizeArray, NumSizes);

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    /* get a list of SDL video modes */
    if (SDL_GetDesktopDisplayMode(get_current_display(), &mode) != 0)
    {
        DebugMessage(M64MSG_ERROR, "SDL_GetDesktopDisplayMode query failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    count = SDL_GetNumDisplayModes(get_current_display());
    if (count <= 0)
    {
        DebugMessage(M64MSG_WARNING, "No fullscreen SDL video modes available");
        *NumSizes = 0;
        return M64ERR_SUCCESS;
    }

    for (i = 0; i < count && i < *NumSizes; i++)
    {
        if (SDL_GetDisplayMode(get_current_display(), i, &mode) == 0)
        { /* assign mode when retrieval is successful */
            SizeArray[i].uiWidth  = mode.w;
            SizeArray[i].uiHeight = mode.h;
        }
        else
        { /* return error when failed */
            DebugMessage(M64MSG_ERROR, "SDL_GetDisplayMode() query failed: %s", SDL_GetError());
            *NumSizes = 0;
            return M64ERR_SYSTEM_FAIL;
        }
    }

    *NumSizes = i;

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ListFullscreenRates(m64p_2d_size Size, int *NumRates, int *Rates)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncListRates)(Size, NumRates, Rates);

    return M64ERR_UNSUPPORTED;
}

EXPORT m64p_error CALL VidExt_SetVideoMode(int Width, int Height, int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
    int windowWidth = 0;
    int windowHeight = 0;
    Uint32 windowFlags = SDL_WINDOW_SHOWN;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncSetMode)(Width, Height, BitsPerPixel, ScreenMode, Flags);
        l_Fullscreen = (rval == M64ERR_SUCCESS && ScreenMode == M64VIDEO_FULLSCREEN);
        l_VideoOutputActive = (rval == M64ERR_SUCCESS);
        if (l_VideoOutputActive)
        {
            StateChanged(M64CORE_VIDEO_MODE, ScreenMode);
            StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
        }
        return rval;
    }

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    /* set window mode */
    if (l_RenderMode == M64P_RENDER_OPENGL)
    {
        windowFlags |= SDL_WINDOW_OPENGL;
    }
    else if (l_RenderMode == M64P_RENDER_VULKAN)
    {
        windowFlags |= SDL_WINDOW_VULKAN;
    }
    if (ScreenMode == M64VIDEO_WINDOWED)
    {
        if (Flags & M64VIDEOFLAG_SUPPORT_RESIZING)
        {
            windowFlags |= SDL_WINDOW_RESIZABLE;
        }
    }
    else if (ScreenMode == M64VIDEO_FULLSCREEN)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    else
    {
        return M64ERR_INPUT_INVALID;
    }

    /* set the mode */
    if (BitsPerPixel > 0)
        DebugMessage(M64MSG_INFO, "Setting %i-bit video mode: %ix%i", BitsPerPixel, Width, Height);
    else
        DebugMessage(M64MSG_INFO, "Setting video mode: %ix%i", Width, Height);

    /* initialize window if needed */
    if (l_pWindow == NULL)
    {
        if (l_RenderMode == M64P_RENDER_OPENGL)
        {
#ifndef USE_GLES
            if (l_ForceCompatibilityContext)
            {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            }
#else
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif // !USE_GLES
        }

        /* set default window title when unset */
        if (l_pWindowTitle == NULL)
            l_pWindowTitle = strdup("Mupen64Plus");

        l_pWindow = SDL_CreateWindow(l_pWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, windowFlags);
        if (l_pWindow == NULL)
        {
            DebugMessage(M64MSG_ERROR, "SDL_CreateWindow failed: %s", SDL_GetError());
            return M64ERR_SYSTEM_FAIL;
        }

        if (l_RenderMode == M64P_RENDER_OPENGL)
        {
            l_pGLContext = SDL_GL_CreateContext(l_pWindow);
            if (SDL_GL_MakeCurrent(l_pWindow, l_pGLContext) != 0)
            {
                DebugMessage(M64MSG_ERROR, "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
                return M64ERR_SYSTEM_FAIL;
            }
        }
    }

    /* resize window if needed */
    SDL_GetWindowSize(l_pWindow, &windowWidth, &windowHeight);
    if (windowWidth != Width || windowHeight != Height)
    {
        SDL_SetWindowSize(l_pWindow, Width, Height);
    }

    SDL_ShowCursor(SDL_DISABLE);

    /* set swap interval/VSync */
    if (l_RenderMode == M64P_RENDER_OPENGL &&
        SDL_GL_SetSwapInterval(l_SwapControl) != 0)
    {
        DebugMessage(M64MSG_ERROR, "SDL swap interval (VSync) set failed: %s", SDL_GetError());
    }

    l_Fullscreen = (ScreenMode == M64VIDEO_FULLSCREEN);
    l_VideoOutputActive = 1;
    StateChanged(M64CORE_VIDEO_MODE, ScreenMode);
    StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_SetVideoModeWithRate(int Width, int Height, int RefreshRate, int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncSetModeWithRate)(Width, Height, RefreshRate, BitsPerPixel, ScreenMode, Flags);
        l_Fullscreen = (rval == M64ERR_SUCCESS && ScreenMode == M64VIDEO_FULLSCREEN);
        l_VideoOutputActive = (rval == M64ERR_SUCCESS);
        if (l_VideoOutputActive)
        {
            StateChanged(M64CORE_VIDEO_MODE, ScreenMode);
            StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
        }
        return rval;
    }

    return M64ERR_UNSUPPORTED;
}

EXPORT m64p_error CALL VidExt_ResizeWindow(int Width, int Height)
{
    int windowWidth = 0;
    int windowHeight = 0;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval;
        // shut down the OSD
        osd_exit();
        // re-create the OGL context
        rval = (*l_ExternalVideoFuncTable.VidExtFuncResizeWindow)(Width, Height);
        if (rval == M64ERR_SUCCESS)
        {
            StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
            // re-create the On-Screen Display
            osd_init(Width, Height);
        }
        return rval;
    }

    if (!l_pWindow || !SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    if (l_Fullscreen)
    {
        DebugMessage(M64MSG_ERROR, "VidExt_ResizeWindow() called in fullscreen mode.");
        return M64ERR_INVALID_STATE;
    }

    SDL_GetWindowSize(l_pWindow, &windowWidth, &windowHeight);
    if (windowHeight != Width || windowHeight != Height)
    {
        SDL_SetWindowSize(l_pWindow, Width, Height);
    }

    StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_SetCaption(const char *Title)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncSetCaption)(Title);

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    /* set window title if window exists, else 
     * store it for later when initializing
     * the window */
    if (l_pWindow)
    {
        SDL_SetWindowTitle(l_pWindow, Title);
    }
    else
    {
        if (l_pWindowTitle != NULL)
            free(l_pWindowTitle);
    
        l_pWindowTitle = strdup(Title);
    }

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ToggleFullScreen(void)
{
    Uint32 windowFlags = 0;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncToggleFS)();
        if (rval == M64ERR_SUCCESS)
        {
            l_Fullscreen = !l_Fullscreen;
            StateChanged(M64CORE_VIDEO_MODE, l_Fullscreen ? M64VIDEO_FULLSCREEN : M64VIDEO_WINDOWED);
        }
        return rval;
    }

    if (!l_pWindow || !SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    /* set correct flags */
    if (!(SDL_GetWindowFlags(l_pWindow) & SDL_WINDOW_FULLSCREEN))
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }

    if (SDL_SetWindowFullscreen(l_pWindow, windowFlags) == 0)
    {
        l_Fullscreen = !l_Fullscreen;
        StateChanged(M64CORE_VIDEO_MODE, l_Fullscreen ? M64VIDEO_FULLSCREEN : M64VIDEO_WINDOWED);
        return M64ERR_SUCCESS;
    }
    else
    {
        DebugMessage(M64MSG_ERROR, "SDL_SetWindowFullscreen failed: %s", SDL_GetError());
    }

    return M64ERR_SYSTEM_FAIL;
}

EXPORT m64p_function CALL VidExt_GL_GetProcAddress(const char* Proc)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetProc)(Proc);

    if (l_RenderMode != M64P_RENDER_OPENGL)
        return NULL;

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return NULL;

/* WARN: assume cast to m64p_function is supported by platform and disable warning accordingly */
OSAL_WARNING_PUSH
OSAL_NO_WARNING_FPTR_VOIDP_CAST
    return (m64p_function)SDL_GL_GetProcAddress(Proc);
OSAL_WARNING_POP
}

typedef struct {
    m64p_GLattr m64Attr;
    SDL_GLattr sdlAttr;
} GLAttrMapNode;

static const GLAttrMapNode GLAttrMap[] = {
        { M64P_GL_DOUBLEBUFFER, SDL_GL_DOUBLEBUFFER },
        { M64P_GL_BUFFER_SIZE,  SDL_GL_BUFFER_SIZE },
        { M64P_GL_DEPTH_SIZE,   SDL_GL_DEPTH_SIZE },
        { M64P_GL_RED_SIZE,     SDL_GL_RED_SIZE },
        { M64P_GL_GREEN_SIZE,   SDL_GL_GREEN_SIZE },
        { M64P_GL_BLUE_SIZE,    SDL_GL_BLUE_SIZE },
        { M64P_GL_ALPHA_SIZE,   SDL_GL_ALPHA_SIZE },
        { M64P_GL_MULTISAMPLEBUFFERS, SDL_GL_MULTISAMPLEBUFFERS },
        { M64P_GL_MULTISAMPLESAMPLES, SDL_GL_MULTISAMPLESAMPLES }
       ,{ M64P_GL_CONTEXT_MAJOR_VERSION, SDL_GL_CONTEXT_MAJOR_VERSION },
        { M64P_GL_CONTEXT_MINOR_VERSION, SDL_GL_CONTEXT_MINOR_VERSION },
        { M64P_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_MASK }
};
static const int mapSize = sizeof(GLAttrMap) / sizeof(GLAttrMapNode);

EXPORT m64p_error CALL VidExt_GL_SetAttribute(m64p_GLattr Attr, int Value)
{
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLSetAttr)(Attr, Value);

    if (l_RenderMode != M64P_RENDER_OPENGL)
        return M64ERR_INVALID_STATE;

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    if (Attr == M64P_GL_SWAP_CONTROL)
    {
        /* SDL swap interval/vsync needs to be set on current GL context, so save it for later */
        l_SwapControl = Value;
    }

    /* translate the GL context type mask if necessary */
    if (Attr == M64P_GL_CONTEXT_PROFILE_MASK)
    {
        switch (Value)
        {
            case M64P_GL_CONTEXT_PROFILE_CORE:
                Value = SDL_GL_CONTEXT_PROFILE_CORE;
#ifndef USE_GLES
                l_ForceCompatibilityContext = 0;
#endif
                break;
            case M64P_GL_CONTEXT_PROFILE_COMPATIBILITY:
                Value = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
                break;
            case M64P_GL_CONTEXT_PROFILE_ES:
                Value = SDL_GL_CONTEXT_PROFILE_ES;
                break;
            default:
                Value = 0;
        }
    }

    for (i = 0; i < mapSize; i++)
    {
        if (GLAttrMap[i].m64Attr == Attr)
        {
            if (SDL_GL_SetAttribute(GLAttrMap[i].sdlAttr, Value) != 0)
                return M64ERR_SYSTEM_FAIL;
            return M64ERR_SUCCESS;
        }
    }

    return M64ERR_INPUT_INVALID;
}

EXPORT m64p_error CALL VidExt_GL_GetAttribute(m64p_GLattr Attr, int *pValue)
{
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetAttr)(Attr, pValue);

    if (l_RenderMode != M64P_RENDER_OPENGL)
        return M64ERR_INVALID_STATE;

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    if (Attr == M64P_GL_SWAP_CONTROL)
    {
        *pValue = SDL_GL_GetSwapInterval();
        return M64ERR_SUCCESS;
    }

    for (i = 0; i < mapSize; i++)
    {
        if (GLAttrMap[i].m64Attr == Attr)
        {
            int NewValue = 0;
            if (SDL_GL_GetAttribute(GLAttrMap[i].sdlAttr, &NewValue) != 0)
                return M64ERR_SYSTEM_FAIL;
            /* translate the GL context type mask if necessary */
            if (Attr == M64P_GL_CONTEXT_PROFILE_MASK)
            {
                switch (NewValue)
                {
                    case SDL_GL_CONTEXT_PROFILE_CORE:
                        NewValue = M64P_GL_CONTEXT_PROFILE_CORE;
                        break;
                    case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
                        NewValue = M64P_GL_CONTEXT_PROFILE_COMPATIBILITY;
                        break;
                    case SDL_GL_CONTEXT_PROFILE_ES:
                        NewValue = M64P_GL_CONTEXT_PROFILE_ES;
                        break;
                    default:
                        NewValue = 0;
                }
            }
            *pValue = NewValue;
            return M64ERR_SUCCESS;
        }
    }

    return M64ERR_INPUT_INVALID;
}

EXPORT m64p_error CALL VidExt_GL_SwapBuffers(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLSwapBuf)();

    if (l_RenderMode != M64P_RENDER_OPENGL)
        return M64ERR_INVALID_STATE;

    if (!l_pWindow || !SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    SDL_GL_SwapWindow(l_pWindow);
    return M64ERR_SUCCESS;
}

EXPORT uint32_t CALL VidExt_GL_GetDefaultFramebuffer(void)
{
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetDefaultFramebuffer)();

    return 0;
}

EXPORT m64p_error CALL VidExt_VK_GetSurface(void** Surface, void* Instance)
{
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncVKGetSurface)(Surface, Instance);

#ifdef VIDEXT_VULKAN
    VkSurfaceKHR vulkanSurface = VK_NULL_HANDLE;

    if (l_RenderMode != M64P_RENDER_VULKAN)
        return M64ERR_INVALID_STATE;

    if (!SDL_WasInit(SDL_INIT_VIDEO) || !l_pWindow)
        return M64ERR_NOT_INIT;

    if (SDL_Vulkan_CreateSurface(l_pWindow, (VkInstance)Instance, &vulkanSurface) == SDL_FALSE) {
        DebugMessage(M64MSG_ERROR, "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    *Surface = (void*)vulkanSurface;
    return M64ERR_SUCCESS;
#else
    return M64ERR_UNSUPPORTED;
#endif
}

EXPORT m64p_error CALL VidExt_VK_GetInstanceExtensions(const char** Extensions[], uint32_t* NumExtensions)
{
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncVKGetInstanceExtensions)(Extensions, NumExtensions);

#ifdef VIDEXT_VULKAN
    if (l_RenderMode != M64P_RENDER_VULKAN)
        return M64ERR_INVALID_STATE;

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

    unsigned int extensionCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(NULL, &extensionCount, NULL) == SDL_FALSE) {
        DebugMessage(M64MSG_ERROR, "SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    /* ensure names have been freed before allocating it again */
    if (l_VulkanExtensionNames != NULL) {
        free(l_VulkanExtensionNames);
        l_VulkanExtensionNames = NULL;
    }

    l_VulkanExtensionNames = malloc(sizeof(const char*) * extensionCount);
    if (l_VulkanExtensionNames == NULL) {
        DebugMessage(M64MSG_ERROR, "malloc failed");
        return M64ERR_SYSTEM_FAIL;
    }

    if (SDL_Vulkan_GetInstanceExtensions(NULL, &extensionCount, l_VulkanExtensionNames) == SDL_FALSE) {
        DebugMessage(M64MSG_ERROR, "SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    *NumExtensions = extensionCount;
    *Extensions    = l_VulkanExtensionNames;
    return M64ERR_SUCCESS;
#else
    return M64ERR_UNSUPPORTED;
#endif
}
