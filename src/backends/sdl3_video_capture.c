/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - sdl3_video_capture.cpp                                  *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2025 Rosalie Wanders                                    *
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

#include <SDL3/SDL.h>

struct sdl3_video_capture
{
    int init_sdl;

    SDL_Camera* camera;
    SDL_CameraID camera_id;

    char* target_camera_name;

    unsigned int width;
    unsigned int height;
};

#include "backends/api/video_capture_backend.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "api/m64p_config.h"

#include <stdlib.h>

const struct video_capture_backend_interface g_isdl3_video_capture_backend;

static m64p_error sdl3_init(void** vcap, const char* section)
{
    /* initialize data */
    *vcap = malloc(sizeof(struct sdl3_video_capture));
    if (*vcap == NULL)
    {
        return M64ERR_NO_MEMORY;
    }

    memset(*vcap, 0, sizeof(struct sdl3_video_capture));
    struct sdl3_video_capture* sdl = (struct sdl3_video_capture*)(*vcap);

    /* attempt to initialize SDL3 */
    if (!SDL_WasInit(SDL_INIT_CAMERA))
    {
        if (!SDL_Init(SDL_INIT_CAMERA))
        {
            DebugMessage(M64MSG_ERROR, "Failed to initialize SDL camera subsystem: %s", SDL_GetError());
            free(sdl);
            return M64ERR_SYSTEM_FAIL;
        }

        sdl->init_sdl = 1;
    }

    /* default parameters */
    const char* device = NULL;

    if (section && strlen(section) > 0)
    {
        m64p_handle config = NULL;

        if (ConfigOpenSection(section, &config) != M64ERR_SUCCESS)
        {
            DebugMessage(M64MSG_WARNING, "Failed to open video configuration section: %s, falling back to default video device", section);
            return M64ERR_SUCCESS;
        }

        /* set default parameters */
        ConfigSetDefaultString(config, "device", "", "Device name to use for capture or empty for default.");

        /* get parameters */
        device = ConfigGetParamString(config, "device");
    }

    /* store device name for later */
    if (device != NULL && strlen(device) > 0)
    {
        sdl->target_camera_name = strdup(device);
    }

    return M64ERR_SUCCESS;
}

static void sdl3_release(void* vcap)
{
    struct sdl3_video_capture* sdl = (struct sdl3_video_capture*)(vcap);
    if (sdl != NULL)
    {
        if (sdl->init_sdl && SDL_WasInit(SDL_INIT_CAMERA))
        {
            SDL_QuitSubSystem(SDL_INIT_CAMERA);
        }

        if (sdl->target_camera_name != NULL)
        {
            free(sdl->target_camera_name);
        }

        free(sdl);
    }
}

static m64p_error sdl3_open(void* vcap, unsigned int width, unsigned int height)
{
    struct sdl3_video_capture* sdl = (struct sdl3_video_capture*)(vcap);
    if (sdl == NULL)
    {
        return M64ERR_NOT_INIT;
    }

    sdl->width = width;
    sdl->height = height;

    int cameras_count = 0;
    SDL_CameraID* cameras = SDL_GetCameras(&cameras_count);
    if (cameras == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Failed to retrieve list of video devices: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }
    if (cameras_count == 0)
    {
        DebugMessage(M64MSG_WARNING, "Failed to find video devices");
        return M64ERR_INPUT_NOT_FOUND;
    }

    /* fallback to default camera */
    sdl->camera_id = cameras[0];

    /* print name of every camera to user, and 
     * attempt to find the camera that the user
     * specified in the config */
    int found_camera = 0;
    for (int i = 0; i < cameras_count; i++)
    {
        const char* name = SDL_GetCameraName(cameras[i]);
        if (name == NULL)
        {
            DebugMessage(M64MSG_ERROR, "Failed to retrieve video device name: %s", SDL_GetError());
            continue;
        }

        DebugMessage(M64MSG_INFO, "Found video device: \"%s\"", name);

        if (sdl->target_camera_name != NULL &&
            strcmp(sdl->target_camera_name, name) == 0)
        {
            sdl->camera_id = cameras[i];
            found_camera = 1;
            break;
        }
    }

    /* show warning when device was not found */
    if (sdl->target_camera_name != NULL && !found_camera)
    {
        DebugMessage(M64MSG_WARNING, "Failed to find video device with name \"%s\", falling back to default", sdl->target_camera_name);
    }

    SDL_free(cameras);

    /* attempt to open camera */
    sdl->camera = SDL_OpenCamera(sdl->camera_id, NULL);
    if (sdl->camera == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Failed to open video device: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    /* attempt to get permission for the camera */
    int permission_state = SDL_GetCameraPermissionState(sdl->camera);
    if (permission_state == 0)
    {
        DebugMessage(M64MSG_INFO, "Waiting until user has approved video access");
        do
        {
            SDL_Delay(250);
            permission_state = SDL_GetCameraPermissionState(sdl->camera);
        } while (permission_state == 0);
    }

    if (permission_state == -1)
    {
        DebugMessage(M64MSG_ERROR, "Failed to open video device: permission denied");
        return M64ERR_SYSTEM_FAIL;
    }

    DebugMessage(M64MSG_INFO, "Video successfully opened: %s", SDL_GetCameraName(sdl->camera_id));
    return M64ERR_SUCCESS;
}

static void sdl3_close(void* vcap)
{
    struct sdl3_video_capture* sdl = (struct sdl3_video_capture*)(vcap);
    if (sdl == NULL)
    {
        return;
    }

    if (sdl->camera != NULL)
    {
        SDL_CloseCamera(sdl->camera);
    }

    DebugMessage(M64MSG_INFO, "Video closed");
}

static m64p_error sdl3_grab_image(void* vcap, void* data)
{
    struct sdl3_video_capture* sdl = (struct sdl3_video_capture*)(vcap);
    if (sdl == NULL || sdl->camera == NULL)
    {
        return M64ERR_NOT_INIT;
    }

    SDL_Surface* frame_surface = SDL_AcquireCameraFrame(sdl->camera, NULL);
    if (frame_surface == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Failed to grab video frame: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    SDL_Surface* target_surface = SDL_CreateSurface(sdl->width, sdl->height, SDL_PIXELFORMAT_BGR24);
    if (target_surface == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Failed to create target surface: %s\n", SDL_GetError());
        SDL_ReleaseCameraFrame(sdl->camera, frame_surface);
        return M64ERR_SYSTEM_FAIL;
    }

    int frame_size = SDL_min(frame_surface->w, frame_surface->h);
    SDL_Rect frame_rect;
    frame_rect.x = (frame_surface->w / 2) - (frame_size / 2);
    frame_rect.y = 0;
    frame_rect.w = frame_size;
    frame_rect.h = frame_size;

    if (!SDL_BlitSurfaceScaled(frame_surface, &frame_rect, target_surface, NULL, SDL_SCALEMODE_NEAREST))
    {
        DebugMessage(M64MSG_ERROR, "Failed to blit surface: %s", SDL_GetError());
        SDL_ReleaseCameraFrame(sdl->camera, frame_surface);
        SDL_DestroySurface(target_surface);
        return M64ERR_SYSTEM_FAIL;
    }

    memcpy(data, target_surface->pixels, target_surface->w * target_surface->h * 3);

    SDL_ReleaseCameraFrame(sdl->camera, frame_surface);
    SDL_DestroySurface(target_surface);
    return M64ERR_SUCCESS;
}



const struct video_capture_backend_interface g_isdl3_video_capture_backend =
{
    "sdl3",
    sdl3_init,
    sdl3_release,
    sdl3_open,
    sdl3_close,
    sdl3_grab_image
};

