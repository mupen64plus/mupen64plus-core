/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - main.c                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Richard42 Ebenblues Nmn Okaygo Tillin9             *
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
 
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#define _7ZIP_UINT32_DEFINED // avoid stupid conflicts between native types and 7zip types
#endif
 
#ifndef __WIN32__
# include <ucontext.h> // extra signal types (for portability)
# include <libgen.h> // basename, dirname
#endif

#include <sys/time.h>
#include <sys/stat.h> /* mkdir() */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  // POSIX macros and standard types.
#include <signal.h> // signals
#include <getopt.h> // getopt_long
#include <dirent.h>
#include <stdlib.h>

#include <png.h>    // for writing screenshot PNG files

#include <SDL.h>
#include <SDL_thread.h>

#include "main.h"
#include "version.h"
#include "config.h"
#include "plugin.h"
#include "rom.h"
#include "savestates.h"
#include "util.h"
#include "translate.h"
#include "cheat.h"

#include "../r4300/r4300.h"
#include "../r4300/recomph.h"
#include "../r4300/interupt.h"

#include "../memory/memory.h"

#include "../osd/osd.h"
#include "../osd/screenshot.h"

#ifdef DBG
#include "../debugger/types.h"
#include "../debugger/debugger.h"
#endif

#ifdef WITH_LIRC
#include "lirc.h"
#endif //WITH_LIRC

#ifdef __APPLE__
// dynamic data path detection onmac
bool macSetBundlePath(char* buffer)
{
    printf("checking whether we are using an app bundle... ");
    // the following code will enable mupen to find its plugins when placed in an app bundle on mac OS X.
    // returns true if path is set, returns false if path was not set
    char path[1024];
    CFBundleRef main_bundle = CFBundleGetMainBundle(); assert(main_bundle);
    CFURLRef main_bundle_URL = CFBundleCopyBundleURL(main_bundle); assert(main_bundle_URL);
    CFStringRef cf_string_ref = CFURLCopyFileSystemPath( main_bundle_URL, kCFURLPOSIXPathStyle); assert(cf_string_ref);
    CFStringGetCString(cf_string_ref, path, 1024, kCFStringEncodingASCII);
    CFRelease(main_bundle_URL);
    CFRelease(cf_string_ref);
    
    if(strstr( path, ".app" ) != 0)
    {
        printf("yes\n");
        // executable is inside an app bundle, use app bundle-relative paths
        sprintf(buffer, "%s/Contents/Resources/", path);
        return true;
    }
    else
    {
        printf("no\n");
        return false;
    }
}
#endif

/** function prototypes **/
static void parseCommandLine(int argc, char **argv);
static int emulationThread( void *_arg );
extern int rom_cache_system( void *_arg );


/** threads **/
SDL_Thread * g_EmulationThread;         // core thread handle
SDL_Thread * g_RomCacheThread;          // rom cache thread handle

/** globals **/
int         g_Noask = 0;                // don't ask to force load on bad dumps
int         g_NoaskParam = 0;           // was --noask passed at the commandline?
int         g_MemHasBeenBSwapped = 0;   // store byte-swapped flag so we don't swap twice when re-playing game
int         g_EmulatorRunning = 0;      // need separate boolean to tell if emulator is running, since --nogui doesn't use a thread
int         g_OsdEnabled = 1;           // On Screen Display enabled?
int         g_Fullscreen = 0;           // fullscreen enabled?
int         g_TakeScreenshot = 0;       // Tell OSD Rendering callback to take a screenshot just before drawing the OSD

char        *g_GfxPlugin = NULL;        // pointer to graphics plugin specified at commandline (if any)
char        *g_AudioPlugin = NULL;      // pointer to audio plugin specified at commandline (if any)
char        *g_InputPlugin = NULL;      // pointer to input plugin specified at commandline (if any)
char        *g_RspPlugin = NULL;        // pointer to rsp plugin specified at commandline (if any)

/** static (local) variables **/
static char l_ConfigDir[PATH_MAX] = {'\0'};
static char l_InstallDir[PATH_MAX] = {'\0'};

static int   l_EmuMode = 0;              // emumode specified at commandline?
static int   l_CurrentFrame = 0;         // frame counter
static int  *l_TestShotList = NULL;      // list of screenshots to take for regression test support
static int   l_TestShotIdx = 0;          // index of next screenshot frame in list
static char *l_Filename = NULL;          // filename to load & run at startup (if given at command line)
static int   l_RomNumber = 0;            // rom number in archive (if given at command line)
static int   l_SpeedFactor = 100;        // percentage of nominal game speed at which emulator is running
static int   l_FrameAdvance = 0;         // variable to check if we pause on next frame

static osd_message_t *l_volMsg = NULL;

/*********************************************************************************************************
* exported gui funcs
*/
char *get_configpath()
{
    return l_ConfigDir;
}

char *get_installpath()
{
    return l_InstallDir;
}

char *get_savespath()
{
    static char path[PATH_MAX];
    strncpy(path, get_configpath(), PATH_MAX-5);
    strcat(path, "save/");
    return path;
}

char *get_iconspath()
{
    static char path[PATH_MAX];
    strncpy(path, get_installpath(), PATH_MAX-6);
    strcat(path, "icons/");
    return path;
}

char *get_iconpath(const char *iconfile)
{
    static char path[PATH_MAX];
    strncpy(path, get_iconspath(), PATH_MAX-strlen(iconfile));
    strcat(path, iconfile);
    return path;
}

void main_message(unsigned int console, unsigned int statusbar, unsigned int osd, unsigned int osd_corner, const char *format, ...)
{
    va_list ap;
    char buffer[2049];
    va_start(ap, format);
    vsnprintf(buffer, 2047, format, ap);
    buffer[2048]='\0';
    va_end(ap);

    if (g_OsdEnabled && osd)
        osd_new_message(osd_corner, buffer);
    if (console)
        printf("%s\n", buffer);
}

void error_message(const char *format, ...)
{
    va_list ap;
    char buffer[2049];
    va_start(ap, format);
    vsnprintf(buffer, 2047, format, ap);
    buffer[2048]='\0';
    va_end(ap);

    printf("%s: %s\n", tr("Error"), buffer);
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
#if defined(__WIN32__)
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

void main_speeddown(int percent)
{
    if (l_SpeedFactor - percent > 10)  /* 10% minimum speed */
    {
        l_SpeedFactor -= percent;
        main_message(0, 1, 1, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_speedup(int percent)
{
    if (l_SpeedFactor + percent < 300) /* 300% maximum speed */
    {
        l_SpeedFactor += percent;
        main_message(0, 1, 1, OSD_BOTTOM_LEFT, "%s %d%%", tr("Playback speed:"), l_SpeedFactor);
        setSpeedFactor(l_SpeedFactor);  // call to audio plugin
    }
}

void main_pause(void)
{
    pauseContinueEmulation();
    l_FrameAdvance = 0;
}

void main_advance_one(void)
{
    l_FrameAdvance = 1;
    rompause = 0;
}

void main_draw_volume_osd(void)
{
    char msgString[32];
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
void take_next_screenshot(void)
{
    g_TakeScreenshot = l_CurrentFrame + 1;
}

void startEmulation(void)
{
    VILimit = GetVILimit();
    VILimitMilliseconds = (double) 1000.0/VILimit; 
    printf("init timer!\n");

    const char *gfx_plugin = NULL,
               *audio_plugin = NULL,
               *input_plugin = NULL,
               *RSP_plugin = NULL;

    // make sure rom is loaded before running
    if(!rom)
    {
        error_message(tr("There is no Rom loaded."));
        return;
    }

    /* Determine which plugins to use:
    *  -If valid plugin was specified at the commandline, use it
    *  -Else, get plugin from config. NOTE: gui code must change config if user switches plugin in the gui)
    */
    if(g_GfxPlugin)
    {
        gfx_plugin = plugin_name_by_filename(g_GfxPlugin);
    }
    else
    {
        gfx_plugin = plugin_name_by_filename(config_get_string("Gfx Plugin", ""));
    }

    if(!gfx_plugin)
    {
        error_message(tr("No graphics plugin specified."));
        return;
    }

    if(g_AudioPlugin)
        audio_plugin = plugin_name_by_filename(g_AudioPlugin);
    else
        audio_plugin = plugin_name_by_filename(config_get_string("Audio Plugin", ""));

    if(!audio_plugin)
    {
        error_message(tr("No audio plugin specified."));
        return;
    }

    if(g_InputPlugin)
        input_plugin = plugin_name_by_filename(g_InputPlugin);
    else
        input_plugin = plugin_name_by_filename(config_get_string("Input Plugin", ""));

    if(!input_plugin)
    {
        error_message(tr("No input plugin specified."));
        return;
    }

    if(g_RspPlugin)
        RSP_plugin = plugin_name_by_filename(g_RspPlugin);
    else
        RSP_plugin = plugin_name_by_filename(config_get_string("RSP Plugin", ""));

    if(!RSP_plugin)
    {
        error_message(tr("No RSP plugin specified."));
        return;
    }

    // load the plugins. Do this outside the emulation thread for GUI
    // related things which cannot be done outside the main thread.
    // Examples: GTK Icon theme setup in Rice which otherwise hangs forever
    // with the Qt4 interface.
    plugin_load_plugins(gfx_plugin, audio_plugin, input_plugin, RSP_plugin);

    // in nogui mode, just start the emulator in the main thread
    emulationThread(NULL);
}

void stopEmulation(void)
{
    if(g_EmulatorRunning)
    {
        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Stopping emulation.\n"));
        rompause = 0;
        stop_it();
#ifdef DBG
        if(debugger_mode)
        {
            
            debugger_step();
        }
#endif        

        // wait until emulation thread is done before continuing
        if(g_EmulatorRunning)
            SDL_WaitThread(g_EmulationThread, NULL);

#ifdef __WIN32__
        plugin_close_plugins();
#endif
        g_EmulatorRunning = 0;
        g_EmulationThread = 0;

        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Emulation stopped.\n"));
    }
}

int pauseContinueEmulation(void)
{
    static osd_message_t *msg = NULL;

    if (!g_EmulatorRunning)
        return 1;

    if (rompause)
    {
        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Emulation continued.\n"));
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

        main_message(0, 1, 0, OSD_BOTTOM_LEFT, tr("Paused\n"));
        msg = osd_new_message(OSD_MIDDLE_CENTER, tr("Paused\n"));
        osd_message_set_static(msg);
    }

    rompause = !rompause;
    return rompause;
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
    if (g_OsdEnabled)
    {
        osd_render();
    }
}

void new_frame(void)
{
    // take a screenshot if we need to
    if (l_TestShotList != NULL)
    {
        int nextshot = l_TestShotList[l_TestShotIdx];
        if (nextshot == l_CurrentFrame)
        {
            // set global variable so screenshot will be taken just before OSD is drawn at the end of frame rendering
            take_next_screenshot();
            // advance list index to next screenshot frame number.  If it's 0, then quit
            l_TestShotIdx++;
        }
        else if (nextshot == 0)
        {
            stopEmulation();
            free(l_TestShotList);
            l_TestShotList = NULL;
        }
    }

    // advance the current frame
    l_CurrentFrame++;
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
#ifdef __WIN32__
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
    static BYTE StopRumble[6] = {0x23, 0x01, 0x03, 0xc0, 0x1b, 0x00};
    char *event_str = NULL;

    switch( event->type )
    {
        // user clicked on window close button
        case SDL_QUIT:
            stopEmulation();
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
            else if (event->key.keysym.sym == config_get_number(kbdStop, SDLK_ESCAPE))
                stopEmulation();
            else if (event->key.keysym.sym == config_get_number(kbdFullscreen, SDLK_LAST))
                changeWindow();
            else if (event->key.keysym.sym == config_get_number(kbdSave, SDLK_F5))
                savestates_job |= SAVESTATE;
            else if (event->key.keysym.sym == config_get_number(kbdLoad, SDLK_F7))
            {
                savestates_job |= LOADSTATE;
                controllerCommand(0, StopRumble);
                controllerCommand(1, StopRumble);
                controllerCommand(2, StopRumble);
                controllerCommand(3, StopRumble);
            }
            else if (event->key.keysym.sym == config_get_number(kbdIncrement, 0))
                savestates_inc_slot();
            else if (event->key.keysym.sym == config_get_number(kbdReset, SDLK_F9))
            {
                add_interupt_event(HW2_INT, 0);  /* Hardware 2 Interrupt immediately */
                add_interupt_event(NMI_INT, 50000000);  /* Non maskable Interrupt after 1/2 second */
            }
            else if (event->key.keysym.sym == config_get_number(kbdSpeeddown, SDLK_F10))
                main_speeddown(5);
            else if (event->key.keysym.sym == config_get_number(kbdSpeedup, SDLK_F11))
                main_speedup(5);
            else if (event->key.keysym.sym == config_get_number(kbdScreenshot, SDLK_F12))
                // set flag so that screenshot will be taken at the end of frame rendering
                take_next_screenshot();
            else if (event->key.keysym.sym == config_get_number(kbdPause, SDLK_p))
                main_pause();
            else if (event->key.keysym.sym == config_get_number(kbdMute, SDLK_m))
            {
                volumeMute();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == config_get_number(kbdIncrease, SDLK_RIGHTBRACKET))
            {
                volumeUp();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == config_get_number(kbdDecrease, SDLK_LEFTBRACKET))
            {
                volumeDown();
                main_draw_volume_osd();
            }
            else if (event->key.keysym.sym == config_get_number(kbdForward, SDLK_f))
            {
                SavedSpeedFactor = l_SpeedFactor;
                l_SpeedFactor = 250;
                setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                // set fast-forward indicator
                msgFF = osd_new_message(OSD_TOP_RIGHT, tr("Fast Forward"));
                osd_message_set_static(msgFF);
            }
            else if (event->key.keysym.sym == config_get_number(kbdAdvance, SDLK_SLASH))
                main_advance_one();
            // pass all other keypresses to the input plugin
            else keyDown( 0, event->key.keysym.sym );

            return 0;

        case SDL_KEYUP:
            if(event->key.keysym.sym == config_get_number(kbdStop, SDLK_ESCAPE))
            {
                return 0;
            }
            else if(event->key.keysym.sym == config_get_number(kbdForward, SDLK_f))
            {
                // cancel fast-forward
                l_SpeedFactor = SavedSpeedFactor;
                setSpeedFactor(l_SpeedFactor);  // call to audio plugin
                // remove message
                osd_delete_message(msgFF);
            }
            else keyUp( 0, event->key.keysym.sym );

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

            if(strcmp(event_str, config_get_string("Joy Mapping Fullscreen", "")) == 0)
                changeWindow();
            else if(strcmp(event_str, config_get_string("Joy Mapping Stop", "")) == 0)
                stopEmulation();
            else if(strcmp(event_str, config_get_string("Joy Mapping Pause", "")) == 0)
                main_pause();
            else if(strcmp(event_str, config_get_string("Joy Mapping Save State", "")) == 0)
                savestates_job |= SAVESTATE;
            else if(strcmp(event_str, config_get_string("Joy Mapping Load State", "")) == 0)
                savestates_job |= LOADSTATE;
            else if(strcmp(event_str, config_get_string("Joy Mapping Increment Slot", "")) == 0)
                savestates_inc_slot();
            else if(strcmp(event_str, config_get_string("Joy Mapping Screenshot", "")) == 0)
                take_next_screenshot();
            else if(strcmp(event_str, config_get_string("Joy Mapping Mute", "")) == 0)
            {
                volumeMute();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, config_get_string("Joy Mapping Decrease Volume", "")) == 0)
            {
                volumeDown();
                main_draw_volume_osd();
            }
            else if(strcmp(event_str, config_get_string("Joy Mapping Increase Volume", "")) == 0)
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
static int emulationThread( void *_arg )
{
#if !defined(__WIN32__)
    struct sigaction sa;
#endif
    const char *gfx_plugin = NULL,
               *audio_plugin = NULL,
           *input_plugin = NULL,
           *RSP_plugin = NULL;

    g_EmulatorRunning = 1;
    // if emu mode wasn't specified at the commandline, set from config file
    if(!l_EmuMode)
        dynacore = config_get_number( "Core", CORE_DYNAREC );
#ifdef NO_ASM
    if(dynacore==CORE_DYNAREC)
        dynacore = CORE_INTERPRETER;
#endif

    no_compiled_jump = config_get_bool("NoCompiledJump", FALSE);

    // init sdl
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
    if (g_Fullscreen)
        changeWindow();

    if (g_OsdEnabled)
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

    if (g_OsdEnabled)
    {
        osd_exit();
    }

    cheat_unload_current_rom();
    romClosed_RSP();
    romClosed_input();
    romClosed_audio();
    romClosed_gfx();
    closeDLL_RSP();
    closeDLL_input();
    closeDLL_audio();
    closeDLL_gfx();
    free_memory();

    // clean up
    g_EmulatorRunning = 0;

    SDL_Quit();

    return 0;
}

static void printUsage(const char *progname)
{
    char *str = strdup(progname);

    printf("Usage: %s [parameter(s)] [romfile]\n"
           "\n"
           "Parameters:\n"
           "    --noask               : do not prompt user if rom file is hacked or a bad dump.\n"
           "    --noosd               : disable onscreen display.\n"
           "    --fullscreen          : turn fullscreen mode on.\n"
           "    --romnumber (number)  : specify which rom in romfile, if multirom archive.\n"
           "    --gfx (plugin-file)   : use gfx plugin given by (path)\n"
           "    --audio (plugin-file) : use audio plugin given by (path)\n"
           "    --input (plugin-file) : use input plugin given by (path)\n"
           "    --rsp (plugin-file)   : use rsp plugin given by (path)\n"
           "    --emumode (mode)      : set emu mode to: 0=Interpreter 1=DynaRec 2=Pure Interpreter\n"
           "    --sshotdir (dir)      : set screenshot directory to (dir)\n"
           "    --configdir (dir)     : force config dir (must contain mupen64plus.conf)\n"
           "    --installdir (dir)    : force install dir (place to look for plugins, icons, lang, etc)\n"
           "    --testshots (list)    : take screenshots at frames given in comma-separated (list), then quit\n"
#ifdef DBG
           "    --debugger            : start with debugger enabled\n"
#endif 
           "    -h, --help            : see this help message\n"
           "\n", str);

    free(str);

    return;
}

/* parseCommandLine
 *  Parses commandline options and sets global variables accordingly
 */
void parseCommandLine(int argc, char **argv)
{
    int i, shots;
    char *str = NULL;

    // option parsing vars
    int opt, option_index;
    enum
    {
        OPT_GFX = 1,
        OPT_AUDIO,
        OPT_INPUT,
        OPT_RSP,
        OPT_EMUMODE,
        OPT_SSHOTDIR,
        OPT_CONFIGDIR,
        OPT_INSTALLDIR,
#ifdef DBG
    OPT_DEBUGGER,
#endif
        OPT_NOASK,
        OPT_TESTSHOTS,
        OPT_ROMNUMBER
    };
    struct option long_options[] =
    {
        {"noosd", no_argument, &g_OsdEnabled, FALSE},
        {"fullscreen", no_argument, &g_Fullscreen, TRUE},
        {"romnumber", required_argument, NULL, OPT_ROMNUMBER},
        {"gfx", required_argument, NULL, OPT_GFX},
        {"audio", required_argument, NULL, OPT_AUDIO},
        {"input", required_argument, NULL, OPT_INPUT},
        {"rsp", required_argument, NULL, OPT_RSP},
        {"emumode", required_argument, NULL, OPT_EMUMODE},
        {"sshotdir", required_argument, NULL, OPT_SSHOTDIR},
        {"configdir", required_argument, NULL, OPT_CONFIGDIR},
        {"installdir", required_argument, NULL, OPT_INSTALLDIR},
#ifdef DBG
        {"debugger", no_argument, NULL, OPT_DEBUGGER},
#endif
        {"noask", no_argument, NULL, OPT_NOASK},
        {"testshots", required_argument, NULL, OPT_TESTSHOTS},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}    // last opt must be empty
    };
    char opt_str[] = "h";

    /* parse commandline options */
    while((opt = getopt_long(argc, argv, opt_str,
                 long_options, &option_index)) != -1)
    {
        switch(opt)
        {
            // if getopt_long returns 0, it already set the global for us, so do nothing
            case 0:
                break;
            case OPT_GFX:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_GFX))
                {
                    g_GfxPlugin = optarg;
                }
                else
                {
                    printf("***Warning: GFX Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_AUDIO:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_AUDIO))
                {
                    g_AudioPlugin = optarg;
                }
                else
                {
                    printf("***Warning: Audio Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_INPUT:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_CONTROLLER))
                {
                    g_InputPlugin = optarg;
                }
                else
                {
                    printf("***Warning: Input Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_RSP:
                if(plugin_scan_file(optarg, PLUGIN_TYPE_RSP))
                {
                    g_RspPlugin = optarg;
                }
                else
                {
                    printf("***Warning: RSP Plugin '%s' couldn't be loaded!\n", optarg);
                }
                break;
            case OPT_EMUMODE:
                i = atoi(optarg);
                if(i >= CORE_INTERPRETER && i <= CORE_PURE_INTERPRETER)
                {
                    l_EmuMode = TRUE;
                    dynacore = i;
                }
                else
                {
                    printf("***Warning: Invalid Emumode: %s\n", optarg);
                }
                break;
            case OPT_SSHOTDIR:
                if(isdir(optarg))
                    SetScreenshotDir(optarg);
                else
                    printf("***Warning: Screen shot directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_CONFIGDIR:
                if(isdir(optarg))
                    strncpy(l_ConfigDir, optarg, PATH_MAX);
                else
                    printf("***Warning: Config directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_INSTALLDIR:
                if(isdir(optarg))
                    strncpy(l_InstallDir, optarg, PATH_MAX);
                else
                    printf("***Warning: Install directory '%s' is not accessible or not a directory.\n", optarg);
                break;
            case OPT_NOASK:
                g_Noask = g_NoaskParam = TRUE;
                break;
            case OPT_TESTSHOTS:
                // count the number of integers in the list
                shots = 1;
                str = optarg;
                while ((str = strchr(str, ',')) != NULL)
                {
                    str++;
                    shots++;
                }
                // create a list and populate it with the frame counter values at which to take screenshots
                if ((l_TestShotList = malloc(sizeof(int) * (shots + 1))) != NULL)
                {
                    int idx = 0;
                    str = optarg;
                    while (str != NULL)
                    {
                        l_TestShotList[idx++] = atoi(str);
                        str = strchr(str, ',');
                        if (str != NULL) str++;
                    }
                    l_TestShotList[idx] = 0;
                }
                break;
#ifdef DBG
            case OPT_DEBUGGER:
                g_DebuggerEnabled = TRUE;
                break;
#endif
            case OPT_ROMNUMBER:
                l_RomNumber = atoi(optarg);
                break;
            // print help
            case 'h':
            case '?':
            default:
                printUsage(argv[0]);
                exit(1);
                break;
        }
    }

    // if there are still parameters left after option parsing, assume it's the rom filename
    if(optind < argc)
    {
        l_Filename = argv[optind];
    }

}

/** setPaths
 *  setup paths to config/install/screenshot directories. The config dir is the dir where all
 *  user config information is stored, e.g. mupen64plus.conf, save files, and plugin conf files.
 *  The install dir is where mupen64plus looks for common files, e.g. plugins, icons, language
 *  translation files.
 */
static void setPaths(void)
{
#ifdef __WIN32__
    strncpy(l_ConfigDir, "./", PATH_MAX);
    strncpy(l_InstallDir, "./", PATH_MAX);
    return;
#else
    char buf[PATH_MAX], buf2[PATH_MAX];

    // if the config dir was not specified at the commandline, look for ~/.mupen64plus dir
    if (strlen(l_ConfigDir) == 0)
    {
        strncpy(l_ConfigDir, getenv("HOME"), PATH_MAX);
        strncat(l_ConfigDir, "/.mupen64plus", PATH_MAX - strlen(l_ConfigDir));

        // if ~/.mupen64plus dir is not found, create it
        if(!isdir(l_ConfigDir))
        {
            printf("Creating %s to store user data\n", l_ConfigDir);
            if(mkdir(l_ConfigDir, (mode_t)0755) != 0)
            {
                printf("Error: Could not create %s: ", l_ConfigDir);
                perror(NULL);
                exit(errno);
            }

            // create save subdir
            strncpy(buf, l_ConfigDir, PATH_MAX);
            strncat(buf, "/save", PATH_MAX - strlen(buf));
            if(mkdir(buf, (mode_t)0755) != 0)
            {
                // report error, but don't exit
                printf("Warning: Could not create %s: %s", buf, strerror(errno));
            }

            // create screenshots subdir
            strncpy(buf, l_ConfigDir, PATH_MAX);
            strncat(buf, "/screenshots", PATH_MAX - strlen(buf));
            if(mkdir(buf, (mode_t)0755) != 0)
            {
                // report error, but don't exit
                printf("Warning: Could not create %s: %s", buf, strerror(errno));
            }
        }
    }

    // make sure config dir has a '/' on the end.
    if(l_ConfigDir[strlen(l_ConfigDir)-1] != '/')
        strncat(l_ConfigDir, "/", PATH_MAX - strlen(l_ConfigDir));

    // if install dir was not specified at the commandline, look for it in the executable's directory
    if (strlen(l_InstallDir) == 0)
    {
#ifdef __APPLE__
        macSetBundlePath(buf);
        strncpy(l_InstallDir, buf, PATH_MAX);
        strncat(buf, "/config/mupen64plus.conf", PATH_MAX - strlen(buf));
#else
        buf[0] = '\0';
        int n = readlink("/proc/self/exe", buf, PATH_MAX);
        if (n > 0)
        {
            buf[n] = '\0';
            dirname(buf);
            strncpy(l_InstallDir, buf, PATH_MAX);
            strncat(buf, "/config/mupen64plus.conf", PATH_MAX - strlen(buf));
        }
#endif
        // if it's not in the executable's directory, try a couple of default locations
        if (buf[0] == '\0' || !isfile(buf))
        {
            strcpy(l_InstallDir, "/usr/local/share/mupen64plus");
            strcpy(buf, l_InstallDir);
            strcat(buf, "/config/mupen64plus.conf");
            if (!isfile(buf))
            {
                strcpy(l_InstallDir, "/usr/share/mupen64plus");
                strcpy(buf, l_InstallDir);
                strcat(buf, "/config/mupen64plus.conf");
                // if install dir is not in the default locations, try the same dir as the binary
                if (!isfile(buf))
                {
                    // try cwd as last resort
                    getcwd(l_InstallDir, PATH_MAX);
                }
            }
        }
    }

    // make sure install dir has a '/' on the end.
    if(l_InstallDir[strlen(l_InstallDir)-1] != '/')
        strncat(l_InstallDir, "/", PATH_MAX - strlen(l_InstallDir));

    // make sure install dir is valid
    strncpy(buf, l_InstallDir, PATH_MAX);
    strncat(buf, "config/mupen64plus.conf", PATH_MAX - strlen(buf));
    if(!isfile(buf))
    {
        printf("Could not locate valid install directory\n");
        exit(1);
    }

    // check user config dir for mupen64plus.conf file. If it's not there, copy all
    // config files from install dir over to user dir.
    strncpy(buf, l_ConfigDir, PATH_MAX);
    strncat(buf, "mupen64plus.conf", PATH_MAX - strlen(buf));
    if(!isfile(buf))
    {
        DIR *dir;
        struct dirent *entry;

        strncpy(buf, l_InstallDir, PATH_MAX);
        strncat(buf, "config", PATH_MAX - strlen(buf));
        dir = opendir(buf);

        // should never hit this error because of previous checks
        if(!dir)
        {
            perror(buf);
            return;
        }

        while((entry = readdir(dir)) != NULL)
        {
            strncpy(buf, l_InstallDir, PATH_MAX);
            strncat(buf, "config/", PATH_MAX - strlen(buf));
            strncat(buf, entry->d_name, PATH_MAX - strlen(buf));

            // only copy regular files
            if(isfile(buf))
            {
                strncpy(buf2, l_ConfigDir, PATH_MAX);
                strncat(buf2, entry->d_name, PATH_MAX - strlen(buf2));

                printf("Copying %s to %s\n", buf, l_ConfigDir);
                if(copyfile(buf, buf2) != 0)
                    printf("Error copying file\n");
            }
        }

        closedir(dir);
    }

    // set screenshot dir if it wasn't specified by the user
    if (!ValidScreenshotDir())
    {
        char chDefaultDir[PATH_MAX + 1];
        snprintf(chDefaultDir, PATH_MAX, "%sscreenshots/", l_ConfigDir);
        SetScreenshotDir(chDefaultDir);
    }
#endif /* __WIN32__ */
}

/*********************************************************************************************************
* main function
*/
int main(int argc, char *argv[])
{
    char dirpath[PATH_MAX];
    int i;
    int retval = EXIT_SUCCESS;
    printf(" __  __                         __   _  _   ____  _             \n");  
    printf("|  \\/  |_   _ _ __   ___ _ __  / /_ | || | |  _ \\| |_   _ ___ \n");
    printf("| |\\/| | | | | '_ \\ / _ \\ '_ \\| '_ \\| || |_| |_) | | | | / __|  \n");
    printf("| |  | | |_| | |_) |  __/ | | | (_) |__   _|  __/| | |_| \\__ \\  \n");
    printf("|_|  |_|\\__,_| .__/ \\___|_| |_|\\___/   |_| |_|   |_|\\__,_|___/  \n");
    printf("             |_|         http://code.google.com/p/mupen64plus/  \n");
    printf("Version %s\n\n",MUPEN_VERSION);

    parseCommandLine(argc, argv);
    setPaths();
    config_read();

    // init multi-language support
    tr_init();

    cheat_read_config();

    // try to get plugin folder path from the mupen64plus config file (except on mac where app bundles may be used)
    strncpy(dirpath, config_get_string("PluginDirectory", ""), PATH_MAX-1);
        
    dirpath[PATH_MAX-1] = '\0';
    // if it's not set in the config file, use the /plugins/ sub-folder of the installation directory
    if (strlen(dirpath) < 2)
    {
        strncpy(dirpath, l_InstallDir, PATH_MAX);
        strncat(dirpath, "plugins/", PATH_MAX - strlen(dirpath));
        dirpath[PATH_MAX-1] = '\0';
    }
    // scan the plugin directory and set the config dir for the plugins
    plugin_scan_directory(dirpath);
    plugin_set_dirs(l_ConfigDir, l_InstallDir);

    // must be called after building gui
    // look for plugins in the install dir and set plugin config dir
    savestates_set_autoinc_slot(config_get_bool("AutoIncSaveSlot", FALSE));

    if((i=config_get_number("CurrentSaveSlot",10))!=10)
    {
        savestates_select_slot((unsigned int)i);
    }
    else
    {
        config_put_number("CurrentSaveSlot",0);
    }

    main_message(1, 0, 0, 0, tr("Config Dir:  %s\nInstall Dir: %s\nPlugin Dir:  %s\n"), l_ConfigDir, l_InstallDir, dirpath);
    main_message(0, 1, 0, 0, tr("Config Dir: \"%s\", Install Dir: \"%s\", Plugin Dir:  \"%s\""), l_ConfigDir, l_InstallDir, dirpath);

    //The database needs to be opened regardless of GUI mode.
    romdatabase_open();

    // if rom file was specified, run it
    if (!l_Filename)
    {
        error_message("Rom file must be specified in nogui mode.");
        printUsage(argv[0]);
        retval = 1;
    }
    else
    {
        if (open_rom(l_Filename, l_RomNumber) >= 0)
        {
            startEmulation();
        }
        else
        {
            retval = 1;
        }
    }

    // free allocated memory
    if (l_TestShotList != NULL)
        free(l_TestShotList);

    // cleanup and exit
    config_write();

/**  Disabling as it seems to be causing some problems
 * Maybe some of the "objects are already deleted?
 * Is it required? Cheats should be saved when clicking OK in
 * The cheats dialog.
**/
//    cheat_write_config();
    cheat_delete_all();

    romdatabase_close();
    plugin_delete_list();
    tr_delete_languages();
    config_delete();

    return retval;
}

#ifdef __WIN32__

static const char* programName = "mupen64plus.exe";

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParamarg, int cmdShow)
{
    list_t arguments = NULL;
    list_node_t*  node = NULL;
    int i = 0;
    char* arg = NULL;
    char* wrk = NULL;
    char** argv = NULL;

    g_ProgramInstance = instance;
    
    wrk = malloc(strlen(programName) + 1);
    strcpy(wrk, programName);
    list_append(&arguments, wrk);
    
    for (arg = strtok(cmdParamarg, " ");
         arg != NULL;
         arg = strtok(NULL, " "))
    {
        wrk = malloc(strlen(arg) + 1);
        strcpy(wrk, arg);
        list_append(&arguments, arg);
    }
    
    argv = malloc(list_length(arguments) + 1 * sizeof(char*));
    list_foreach(arguments, node)
    {
        argv[i++] = node->data;
    }
    
    return main(i, argv);
}
#endif

