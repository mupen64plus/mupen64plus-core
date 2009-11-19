/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eventloop.c                                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008-2009 Richard Goedeken                              *
 *   Copyright (C) 2008 Ebenblues Nmn Okaygo Tillin9                       *
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
#include <SDL.h>

#include "main.h"
#include "savestates.h"
#include "util.h"
#include "api/config.h"
#include "plugin/plugin.h"
#include "r4300/interupt.h"

/*********************************************************************************************************
* static functions for eventloop.c
*/

/** event_to_str
 *    Creates a string representation of an SDL input event. If the event is
 *    not supported by this function, NULL is returned.
 *
 *    Notes:
 *     -This function assumes SDL events are already initialized.
 *     -It is up to the caller to free the string memory allocated by this
 *      function.
 */
static char *event_to_str(const SDL_Event *event)
{
    char *event_str = NULL;

    switch(event->type)
    {
        case SDL_JOYAXISMOTION:
            if(event->jaxis.value >= 15000 || event->jaxis.value <= -15000)
            {
                event_str = malloc(10);
                snprintf(event_str, 10, "J%dA%d%c",
                         event->jaxis.which,
                     event->jaxis.axis,
                     event->jaxis.value > 0? '+' : '-');
            }
            break;

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            event_str = malloc(10);
            snprintf(event_str, 10, "J%dB%d",
                     event->jbutton.which,
                     event->jbutton.button);
            break;

        case SDL_JOYHATMOTION:
            event_str = malloc(10);
            snprintf(event_str, 10, "J%dH%dV%d",
                     event->jhat.which,
                     event->jhat.hat,
                     event->jhat.value);
            break;
    }

    return event_str;
}

/*********************************************************************************************************
* global functions for testing if certain events are active
*/

/** event_active
 *    Returns 1 if the specified joystick event is currently active. This
 *    function expects an input string of the same form output by event_to_str.
 */
int event_active(const char* event_str)
{
    char device, joy_input_type, axis_direction;
    int dev_number, input_number, input_value;
    SDL_Joystick *joystick = NULL;

    /* Empty string. */
    if(!event_str||strlen(event_str)==0)
        return 0;

    /* Parse string depending on type of joystick input. */
    if(event_str[0]=='J')
        {
        switch(event_str[2])
            {
            /* Axis. */
            case 'A':
                sscanf(event_str, "%c%d%c%d%c", &device, &dev_number,
                       &joy_input_type, &input_number, &axis_direction);
                break;
            /* Hat. ??? */
            case 'H':
                sscanf(event_str, "%c%d%c%dV%d", &device, &dev_number,
                       &joy_input_type, &input_number, &input_value);
                break;
            /* Button. */
            case 'B':
                sscanf(event_str, "%c%d%c%d", &device, &dev_number,
                       &joy_input_type, &input_number);
                break;
            }

        joystick = SDL_JoystickOpen(dev_number);
        SDL_JoystickUpdate();
        switch(joy_input_type)
            {
            case 'A':
                if(axis_direction=='-')
                    return SDL_JoystickGetAxis(joystick, input_number) < -15000;
                else
                    return SDL_JoystickGetAxis(joystick, input_number) > 15000;
                return (int)SDL_JoystickGetButton(joystick, input_number);
                break;
            case 'B':
                return (int)SDL_JoystickGetButton(joystick, input_number);
                break;
            case 'H':
                return SDL_JoystickGetHat(joystick, input_number) == input_value;
                break;
            }
        }

    /* TODO: Keyboard event. */
    /* if(event_str[0]=='K') */

    /* Undefined event. */
    return 0;
}

/** key_pressed
 *   Returns 1 if the given key is currently pressed.
 */
int key_pressed(int k)
{
    Uint8 *key_states;
    int num_keys;

    SDL_PumpEvents(); // update input state array
    key_states = SDL_GetKeyState(&num_keys);

    if(k >= num_keys)
        return 0;

    return key_states[k];
}

/*********************************************************************************************************
* definitions and function for setting the event-related configuration defaults in the Core section
*/

#define kbdFullscreen "Kbd Mapping Fullscreen"
#define kbdStop "Kbd Mapping Stop"
#define kbdPause "Kbd Mapping Pause"
#define kbdSave "Kbd Mapping Save State"
#define kbdLoad "Kbd Mapping Load State"
#define kbdIncrement "Kbd Mapping Increment Slot"
#define kbdReset "Kbd Mapping Reset"
#define kbdSpeeddown "Kbd Mapping Speed Down"
#define kbdSpeedup "Kbd Mapping Speed Up"
#define kbdScreenshot "Kbd Mapping Screenshot"
#define kbdMute "Kbd Mapping Mute"
#define kbdIncrease "Kbd Mapping Increase Volume"
#define kbdDecrease "Kbd Mapping Decrease Volume"
#define kbdForward "Kbd Mapping Fast Forward"
#define kbdAdvance "Kbd Mapping Frame Advance"
#define kbdGameshark "Kbd Mapping Gameshark"

#define joyFullscreen "Joy Mapping Fullscreen"
#define joyStop "Joy Mapping Stop"
#define joyPause "Joy Mapping Pause"
#define joySave "Joy Mapping Save State"
#define joyLoad "Joy Mapping Load State"
#define joyIncrement "Joy Mapping Increment Slot"
#define joyScreenshot "Joy Mapping Screenshot"
#define joyMute "Joy Mapping Mute"
#define joyIncrease "Joy Mapping Increase Volume"
#define joyDecrease "Joy Mapping Decrease Volume"
#define joyGameshark "Joy Mapping Gameshark"

void event_set_core_defaults(void)
{
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

/*********************************************************************************************************
* sdl keyup/keydown handlers
*/

void event_sdl_keydown(int keysym, int keymod)
{
    static unsigned char StopRumble[6] = {0x23, 0x01, 0x03, 0xc0, 0x1b, 0x00};

    /* check for the only 2 hard-coded key commands: Alt-enter for fullscreen and 0-9 for save state slot */
    if (keysym == SDLK_RETURN && keymod & (KMOD_LALT | KMOD_RALT))
    {
        changeWindow();
    }
    else if (keysym >= SDLK_0 && keysym <= SDLK_9)
    {
        savestates_select_slot(keysym - SDLK_0);
    }
    /* check all of the configurable commands */
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdStop))
        main_stop();
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdFullscreen))
        changeWindow();
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdSave))
        savestates_job |= SAVESTATE;
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdLoad))
    {
        savestates_job |= LOADSTATE;
        controllerCommand(0, StopRumble);
        controllerCommand(1, StopRumble);
        controllerCommand(2, StopRumble);
        controllerCommand(3, StopRumble);
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdIncrement))
        savestates_inc_slot();
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdReset))
    {
        add_interupt_event(HW2_INT, 0);  /* Hardware 2 Interrupt immediately */
        add_interupt_event(NMI_INT, 50000000);  /* Non maskable Interrupt after 1/2 second */
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdSpeeddown))
        main_speeddown(5);
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdSpeedup))
        main_speedup(5);
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdScreenshot))
        // set flag so that screenshot will be taken at the end of frame rendering
        main_take_next_screenshot();
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdPause))
        main_toggle_pause();
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdMute))
    {
        volumeMute();
        main_draw_volume_osd();
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdIncrease))
    {
        volumeUp();
        main_draw_volume_osd();
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdDecrease))
    {
        volumeDown();
        main_draw_volume_osd();
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdForward))
    {
        main_set_fastforward(1);
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdAdvance))
    {
        main_advance_one();
    }
    else
    {
        /* pass all other keypresses to the input plugin */
        keyDown(keymod, keysym);
    }

}

void event_sdl_keyup(int keysym, int keymod)
{
    if (keysym == ConfigGetParamInt(g_CoreConfig, kbdStop))
    {
        return;
    }
    else if (keysym == ConfigGetParamInt(g_CoreConfig, kbdForward))
    {
        main_set_fastforward(0);
    }
    else keyUp(keymod, keysym);

}

/*********************************************************************************************************
* sdl event filter
*/
int event_sdl_filter(const SDL_Event *event)
{
    char *event_str = NULL;

    switch(event->type)
    {
        // user clicked on window close button
        case SDL_QUIT:
            main_stop();
            break;

        case SDL_KEYDOWN:
            event_sdl_keydown(event->key.keysym.sym, event->key.keysym.mod);
            return 0;
        case SDL_KEYUP:
            event_sdl_keyup(event->key.keysym.sym, event->key.keysym.mod);
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


