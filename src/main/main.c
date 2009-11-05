/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - main.c                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008-2009 Richard Goedeken                              *
 *   Copyright (C) 2008 Ebenblues Nmn Okaygo Tillin9                       *
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

/* This is MUPEN64's main entry point. It contains code that is common
 * to both the gui and non-gui versions of mupen64. See
 * gui subdirectories for the gui-specific code.
 * if you want to implement an interface, you should look here
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <SDL.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/config.h"

#include "osal/files.h"

#include "main.h"
#include "version.h"
#include "rom.h"
#include "savestates.h"
#include "util.h"
#include "translate.h"
#include "cheat.h"

#include "r4300/r4300.h"
#include "r4300/recomph.h"
#include "r4300/interupt.h"

#include "memory/memory.h"

#include "plugin/plugin.h"

#include "osd/osd.h"
#include "osd/screenshot.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/debugger.h"
#endif

#ifdef WITH_LIRC
#include "lirc.h"
#endif //WITH_LIRC

/** globals **/
m64p_handle g_CoreConfig = NULL;

int         g_MemHasBeenBSwapped = 0;   // store byte-swapped flag so we don't swap twice when re-playing game
int         g_EmulatorRunning = 0;      // need separate boolean to tell if emulator is running, since --nogui doesn't use a thread
int         g_TakeScreenshot = 0;       // Tell OSD Rendering callback to take a screenshot just before drawing the OSD

/** static (local) variables **/
static int   l_CurrentFrame = 0;         // frame counter
static int   l_SpeedFactor = 100;        // percentage of nominal game speed at which emulator is running
static int   l_FrameAdvance = 0;         // variable to check if we pause on next frame

static osd_message_t *l_volMsg = NULL;

/*********************************************************************************************************
* helper functions
*/
char *get_savespath()
{
    static char path[1024];

    snprintf(path, 1024, "%ssave%c", ConfigGetUserDataPath(), OSAL_DIR_SEPARATOR);
    path[1023] = 0;

    /* make sure the directory exists */
    if (osal_mkdirp(path, 0700) != 0)
        return NULL;

    return path;
}

void main_message(m64p_msg_level level, unsigned int osd_corner, const char *format, ...)
{
    va_list ap;
    char buffer[2049];
    va_start(ap, format);
    vsnprintf(buffer, 2047, format, ap);
    buffer[2048]='\0';
    va_end(ap);

    /* send message to on-screen-display if enabled */
    if (ConfigGetParamBool(g_CoreConfig, "OnScreenDisplay"))
        osd_new_message(osd_corner, buffer);
    /* send message to front-end */
    DebugMessage(level, buffer);
}


/*********************************************************************************************************
* timer functions
*/
static float VILimit = 60.0;
static double VILimitMilliseconds = 1000.0/60.0;

static int GetVILimit(void)
{
    switch (ROM_HEADER->Country_code&0xFF)
    {
        // PAL codes
        case 0x44:
        case 0x46:
        case 0x49:
        case 0x50:
        case 0x53:
        case 0x55:
        case 0x58:
        case 0x59:
            return 50;
            break;

        // NTSC codes
        case 0x37:
        case 0x41:
        case 0x45:
        case 0x4a:
            return 60;
            break;

        // Fallback for unknown codes
        default:
            return 60;
    }
}

static unsigned int gettimeofday_msec(void)
{
    unsigned int foo;
#if defined(WIN32)
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    GetSystemTimeAsFileTime(&ft);
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
    foo = (((tmpres / 1000000UL) % 1000000) * 1000) + (tmpres % 1000000UL / 1000);
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    foo = ((tv.tv_sec % 1000000) * 1000) + (tv.tv_usec / 1000);
#endif
    return foo;
}

/*********************************************************************************************************
* global functions, for adjusting the core emulator behavior
*/

void main_set_core_defaults(void)
{
    /* parameters controlling the operation of the core */
    ConfigSetDefaultBool(g_CoreConfig, "Fullscreen", 0, "Use fullscreen mode if True, or windowed mode if False ");
    ConfigSetDefaultBool(g_CoreConfig, "OnScreenDisplay", 1, "Draw on-screen display if True, otherwise don't draw OSD");
    ConfigSetDefaultInt(g_CoreConfig, "R4300Emulator", 1, "Use Pure Interpreter if 0, Cached Interpreter if 1, or Dynamic Recompiler if 2 or more");
    ConfigSetDefaultBool(g_CoreConfig, "NoCompiledJump", 0, "Disable compiled jump commands in dynamic recompiler (should be set to False) ");
    ConfigSetDefaultBool(g_CoreConfig, "DisableExtraMem", 0, "Disable 4MB expansion RAM pack. May be necessary for some games");
    ConfigSetDefaultBool(g_CoreConfig, "AutoStateSlotIncrement", 0, "Increment the save state slot after each save operation");
    ConfigSetDefaultInt(g_CoreConfig, "CurrentStateSlot", 0, "Save state slot (0-9) to use when saving/loading the emulator state");
    ConfigSetDefaultString(g_CoreConfig, "ScreenshotPath", "", "Path to directory where screenshots are saved. If this is blank, the default value of ${UserConfigPath}/screenshot will be used");
    ConfigSetDefaultString(g_CoreConfig, "SaveStatePath", "", "Path to directory where save states are saved. If this is blank, the default value of ${UserConfigPath}/save will be used");
    ConfigSetDefaultString(g_CoreConfig, "SharedDataPath", "", "Path to a directory to search when looking for shared data files");
    ConfigSetDefaultString(g_CoreConfig, "Language", "English", "Language to use for messages from the core library");
    /* Keyboard presses mapped to core functions */
    ConfigSetDefaultInt(g_CoreConfig, kbdStop, SDLK_ESCAPE,          "SDL keysym for stopping the emulator");
    ConfigSetDefaultInt(g_CoreConfig, kbdFullscreen, SDLK_LAST,      "SDL keysym for switching between fullscreen/windowed modes");
    ConfigSetDefaultInt(g_CoreConfig, kbdSave, SDLK_F5,              "SDL keysym for saving the emulator state");
    ConfigSetDefaultInt(g_CoreConfig, kbdLoad, SDLK_F7,              "SDL keysym for loading the emulator state");
    ConfigSetDefaultInt(g_CoreConfig, kbdIncrement, 0,               "SDL keysym for advancing the save state slot");
    ConfigSetDefaultInt(g_CoreConfig, kbdReset, SDLK_F9,             "SDL keysym for resetting the emulator");
    ConfigSetDefaultInt(g_CoreConfig, kbdSpeeddown, SDLK_F10,        "SDL keysym for slowing down the emulator");
    ConfigSetDefaultInt(g_CoreConfig, kbdSpeedup, SDLK_F11,          "SDL keysym for speeding up the emulator");
    ConfigSetDefaultInt(g_CoreConfig, kbdScreenshot, SDLK_F12,       "SDL keysym for taking a screenshot");
    ConfigSetDefaultInt(g_CoreConfig, kbdPause, SDLK_p,              "SDL keysym for pausing the emulator");
    ConfigSetDefaultInt(g_CoreConfig, kbdMute, SDLK_m,               "SDL keysym for muting/unmuting the sound");
    ConfigSetDefaultInt(g_CoreConfig, kbdIncrease, SDLK_RIGHTBRACKET,"SDL keysym for increasing the volume");
    ConfigSetDefaultInt(g_CoreConfig, kbdDecrease, SDLK_LEFTBRACKET, "SDL keysym for decreasing the volume");
    ConfigSetDefaultInt(g_CoreConfig, kbdForward, SDLK_f,            "SDL keysym for temporarily going really fast");
    ConfigSetDefaultInt(g_CoreConfig, kbdAdvance, SDLK_SLASH,        "SDL keysym for advancing by one frame when paused");
    ConfigSetDefaultInt(g_CoreConfig, kbdGameshark, SDLK_g,          "SDL keysym for pressing the game shark button");
    /* Joystick events mapped to core functions */
    ConfigSetDefaultString(g_CoreConfig, joyStop, "",       "Joystick event string for stopping the emulator");
    ConfigSetDefaultString(g_CoreConfig, joyFullscreen, "", "Joystick event string for switching between fullscreen/windowed modes");
    ConfigSetDefaultString(g_CoreConfig, joySave, "",       "Joystick event string for saving the emulator state");
    ConfigSetDefaultString(g_CoreConfig, joyLoad, "",       "Joystick event string for loading the emulator state");
    ConfigSetDefaultString(g_CoreConfig, joyIncrement, "",  "Joystick event string for advancing the save state slot");
    ConfigSetDefaultString(g_CoreConfig, joyScreenshot, "", "Joystick event string for taking a screenshot");
    ConfigSetDefaultString(g_CoreConfig, joyPause, "",      "Joystick event string for pausing the emulator");
    ConfigSetDefaultString(g_CoreConfig, joyMute, "",       "Joystick event string for muting/unmuting the sound");
    ConfigSetDefaultString(g_CoreConfig, joyIncrease, "",   "Joystick event string for increasing the volume");
    ConfigSetDefaultString(g_CoreConfig, joyDecrease, "",   "Joystick event string for decreasing the volume");
    ConfigSetDefaultString(g_CoreConfig, joyGameshark, "",  "Joystick event string for pressing the game shark button");
}

void main_speeddown(int percent)
{
    if (l_SpeedFactor - percent > 10)  /* 10% minimum speed */
    {
        l_SpeedFactor -= percent;
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_speedup(int percent)
{
    if (l_SpeedFactor + percent < 300) /* 300% maximum speed */
    {
        l_SpeedFactor += percent;
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_toggle_pause(void)
{
    static osd_message_t *msg = NULL;

    if (!g_EmulatorRunning)
        return;

    if (rompause)
    {
        DebugMessage(M64MSG_STATUS, tr("Emulation continued.\n"));
        if(msg)
        {
            osd_delete_message(msg);
            msg = NULL;
        }
    }
    else
    {
        if(msg)
            osd_delete_message(msg);

        DebugMessage(M64MSG_STATUS, tr("Emulation paused.\n"));
        msg = osd_new_message(OSD_MIDDLE_CENTER, tr("Paused\n"));
        osd_message_set_static(msg);
    }

    rompause = !rompause;
    l_FrameAdvance = 0;
}

void main_advance_one(void)
{
    l_FrameAdvance = 1;
    rompause = 0;
}

void main_draw_volume_osd(void)
{
    char msgString[64];
    const char *volString;

    // if we had a volume message, make sure that it's still in the OSD list, or set it to NULL
    if (l_volMsg != NULL && !osd_message_valid(l_volMsg))
        l_volMsg = NULL;

    // this calls into the audio plugin
    volString = volumeGetString();
    if (volString == NULL)
    {
        strcpy(msgString, tr("Volume Not Supported."));
    }
    else
    {
        sprintf(msgString, "%s: %s", tr("Volume"), volString);
        if (msgString[strlen(msgString) - 1] == '%')
            strcat(msgString, "%");
    }

    // create a new message or update an existing one
    if (l_volMsg != NULL)
        osd_update_message(l_volMsg, msgString);
    else
        l_volMsg = osd_new_message(OSD_MIDDLE_CENTER, msgString);
}

/* this function could be called as a result of a keypress, joystick/button movement,
   LIRC command, or 'testshots' command-line option timer */
void main_take_next_screenshot(void)
{
    g_TakeScreenshot = l_CurrentFrame + 1;
}

/*********************************************************************************************************
* global functions, callbacks from the r4300 core or from other plugins
*/

void video_plugin_render_callback(void)
{
    // if the flag is set to take a screenshot, then grab it now
    if (g_TakeScreenshot != 0)
    {
        TakeScreenshot(g_TakeScreenshot - 1);  // current frame number +1 is in g_TakeScreenshot
        g_TakeScreenshot = 0; // reset flag
    }

    // if the OSD is enabled, then draw it now
    if (ConfigGetParamBool(g_CoreConfig, "OnScreenDisplay"))
    {
        osd_render();
    }
}

void new_frame(void)
{
/* fixme remove
    // take a screenshot if we need to
    if (l_TestShotList != NULL)
    {
        int nextshot = l_TestShotList[l_TestShotIdx];
        if (nextshot == l_CurrentFrame)
        {
            // set global variable so screenshot will be taken just before OSD is drawn at the end of frame rendering
            main_take_next_screenshot();
            // advance list index to next screenshot frame number.  If it's 0, then quit
            l_TestShotIdx++;
        }
        else if (nextshot == 0)
        {
            main_stop();
            free(l_TestShotList);
            l_TestShotList = NULL;
        }
    }

    // advance the current frame
    l_CurrentFrame++;
*/
}

void new_vi(void)
{
    int Dif;
    unsigned int CurrentFPSTime;
    static unsigned int LastFPSTime = 0;
    static unsigned int CounterTime = 0;
    static unsigned int CalculatedTime ;
    static int VI_Counter = 0;

    double AdjustedLimit = VILimitMilliseconds * 100.0 / l_SpeedFactor;  // adjust for selected emulator speed
    int time;

    start_section(IDLE_SECTION);
    VI_Counter++;
    
#ifdef DBG
    if(debugger_mode) debugger_frontend_vi();
#endif

    if(LastFPSTime == 0)
    {
        LastFPSTime = gettimeofday_msec();
        CounterTime = gettimeofday_msec();
        return;
    }
    CurrentFPSTime = gettimeofday_msec();
    
    Dif = CurrentFPSTime - LastFPSTime;
    
    if (Dif < AdjustedLimit) 
    {
        CalculatedTime = CounterTime + AdjustedLimit * VI_Counter;
        time = (int)(CalculatedTime - CurrentFPSTime);
        if (time > 0)
        {
#ifdef WIN32
            Sleep(time);
#else
            usleep(time * 1000);
#endif
        }
        CurrentFPSTime = CurrentFPSTime + time;
    }

    if (CurrentFPSTime - CounterTime >= 1000.0 ) 
    {
        CounterTime = gettimeofday_msec();
        VI_Counter = 0 ;
    }
    
    LastFPSTime = CurrentFPSTime ;
    end_section(IDLE_SECTION);
    if (l_FrameAdvance) {
        rompause = 1;
        l_FrameAdvance = 0;
    }
}

/*********************************************************************************************************
* sdl event filter
*/
static int sdl_event_filter( const SDL_Event *event )
{
    static osd_message_t *msgFF = NULL;
    static int SavedSpeedFactor = 100;
    static unsigned char StopRumble[6] = {0x23, 0x01, 0x03, 0xc0, 0x1b, 0x00};
    char *event_str = NULL;

    switch( event->type )
    {
        // user clicked on window close button
        case SDL_QUIT:
            main_stop();
            break;
        case SDL_KEYDOWN:
            /* check for the only 2 hard-coded key commands: Alt-enter for fullscreen and 0-9 for save state slot */
            if (event->key.keysym.sym == SDLK_RETURN && event->key.keysym.mod & (KMOD_LALT | KMOD_RALT))
            {
                changeWindow();
            }
            else if (event->key.keysym.unicode >= '0' && event->key.keysym.unicode <= '9')
            {
                savestates_select_slot( event->key.keysym.unicode - '0' );
            }
            /* check all of the configurable commands */
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdStop))
                main_stop();
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdFullscreen))
                changeWindow();
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdSave))
                savestates_job |= SAVESTATE;
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdLoad))
            {
                savestates_job |= LOADSTATE;
                controllerCommand(0, StopRumble);
                controllerCommand(1, StopRumble);
                controllerCommand(2, StopRumble);
                controllerCommand(3, StopRumble);
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdIncrement))
                savestates_inc_slot();
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdReset))
            {
                add_interupt_event(HW2_INT, 0);  /* Hardware 2 Interrupt immediately */
                add_interupt_event(NMI_INT, 50000000);  /* Non maskable Interrupt after 1/2 second */
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdSpeeddown))
                main_speeddown(5);
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdSpeedup))
                main_speedup(5);
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdScreenshot))
                // set flag so that screenshot will be taken at the end of frame rendering
                main_take_next_screenshot();
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdPause))
                main_toggle_pause();
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdMute))
            {
                volumeMute();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdIncrease))
            {
                volumeUp();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdDecrease))
            {
                volumeDown();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdForward))
            {
                SavedSpeedFactor = l_SpeedFactor;
                l_SpeedFactor = 250;
                setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                // set fast-forward indicator
                msgFF = osd_new_message(OSD_TOP_RIGHT, tr("Fast Forward"));
                osd_message_set_static(msgFF);
            }
            else if (event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdAdvance))
                main_advance_one();
            // pass all other keypresses to the input plugin
            else keyDown(event->key.keysym.mod, event->key.keysym.sym);

            return 0;

        case SDL_KEYUP:
            if(event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdStop))
            {
                return 0;
            }
            else if(event->key.keysym.sym == ConfigGetParamInt(g_CoreConfig, kbdForward))
            {
                // cancel fast-forward
                l_SpeedFactor = SavedSpeedFactor;
                setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                // remove message
                osd_delete_message(msgFF);
            }
            else keyUp(event->key.keysym.mod, event->key.keysym.sym);

            return 0;

        // if joystick action is detected, check if it's mapped to a special function
        case SDL_JOYAXISMOTION:
            // axis events have to be above a certain threshold to be valid
            if(event->jaxis.value > -15000 && event->jaxis.value < 15000)
                break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYHATMOTION:
            event_str = event_to_str(event);

            if(!event_str) return 0;

            if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyFullscreen)) == 0)
                changeWindow();
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyStop)) == 0)
                main_stop();
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyPause)) == 0)
                main_toggle_pause();
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joySave)) == 0)
                savestates_job |= SAVESTATE;
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyLoad)) == 0)
                savestates_job |= LOADSTATE;
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyIncrement)) == 0)
                savestates_inc_slot();
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyScreenshot)) == 0)
                main_take_next_screenshot();
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyMute)) == 0)
            {
                volumeMute();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyDecrease)) == 0)
            {
                volumeDown();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, ConfigGetParamString(g_CoreConfig, joyIncrease)) == 0)
            {
                volumeUp();
                main_draw_volume_osd();
            }

            free(event_str);
            return 0;
            break;
    }

    return 1;
}

/*********************************************************************************************************
* emulation thread - runs the core
*/
int main_run(void)
{
    VILimit = GetVILimit();
    VILimitMilliseconds = (double) 1000.0/VILimit; 

    g_EmulatorRunning = 1;

    /* take the r4300 emulator mode from the config file at this point and cache it in a global variable */
    r4300emu = ConfigGetParamInt(g_CoreConfig, "R4300Emulator");

    /* set some other core parameters based on the config file values */
    savestates_set_autoinc_slot(ConfigGetParamBool(g_CoreConfig, "AutoStateSlotIncrement"));
    savestates_select_slot(ConfigGetParamInt(g_CoreConfig, "CurrentStateSlot"));
    no_compiled_jump = ConfigGetParamBool(g_CoreConfig, "NoCompiledJump");

    /* fixme this should already have been done when attaching the video plugin */
    /* */
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(0);
    SDL_EnableKeyRepeat(0, 0);
    SDL_SetEventFilter(sdl_event_filter);
    SDL_EnableUNICODE(1);

    // initialize memory, and do byte-swapping if it's not been done yet
    if (g_MemHasBeenBSwapped == 0)
    {
        init_memory(1);
        g_MemHasBeenBSwapped = 1;
    }
    else
    {
        init_memory(0);
    }

    // Attach rom to plugins
    romOpen_gfx();
    romOpen_audio();
    romOpen_input();

    // switch to fullscreen if enabled
    if (ConfigGetParamBool(g_CoreConfig, "Fullscreen"))
        changeWindow();

    if (ConfigGetParamBool(g_CoreConfig, "OnScreenDisplay"))
    {
        // init on-screen display
        void *pvPixels = NULL;
        int width = 640, height = 480;
        readScreen(&pvPixels, &width, &height); // read screen to get width and height
        if (pvPixels != NULL)
        {
            free(pvPixels);
            pvPixels = NULL;
        }
        osd_init(width, height);
    }

    // setup rendering callback from video plugin to the core, for screenshots and On-Screen-Display
    setRenderingCallback(video_plugin_render_callback);

#ifdef WITH_LIRC
    lircStart();
#endif // WITH_LIRC

#ifdef DBG
    /* fixme this needs some communication with front-end to work */
    if (g_DebuggerEnabled)
        init_debugger();
#endif

    /* load cheats for the current rom  */
    cheat_load_current_rom();

    /* Startup message on the OSD */
    osd_new_message(OSD_MIDDLE_CENTER, "Mupen64Plus Started...");

    /* call r4300 CPU core and run the game */
    r4300_reset_hard();
    r4300_reset_soft();
    r4300_execute();

#ifdef WITH_LIRC
    lircStop();
#endif // WITH_LIRC

#ifdef DBG
    if (debugger_mode)
        destroy_debugger();
#endif

    if (ConfigGetParamBool(g_CoreConfig, "OnScreenDisplay"))
    {
        osd_exit();
    }

    cheat_unload_current_rom();
    romClosed_RSP();
    romClosed_input();
    romClosed_audio();
    romClosed_gfx();
    //closeDLL_RSP();
    //closeDLL_input();
    //closeDLL_audio();
    //closeDLL_gfx();
    free_memory();

    // clean up
    g_EmulatorRunning = 0;

    SDL_Quit();

    return 0;
}

void main_stop(void)
{
    /* note: this operation is asynchronous.  It may be called from a thread other than the
       main emulator thread, and may return before the emulator is completely stopped */
    if (!g_EmulatorRunning)
        return;

    DebugMessage(M64MSG_STATUS, tr("Stopping emulation.\n"));
    rompause = 0;
    stop_it();
#ifdef DBG
    if(debugger_mode)
    {
        debugger_step();
    }
#endif        
}

/*********************************************************************************************************
* main function
*/
int main(int argc, char *argv[])
{
    return 1;
}

