/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - main.h                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Blight                                             *
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

#ifndef __MAIN_H__
#define __MAIN_H__

#include "api/m64p_types.h"

/* globals */
extern m64p_handle g_CoreConfig;

extern int g_MemHasBeenBSwapped;
extern int g_TakeScreenshot;
extern int g_EmulatorRunning;

extern m64p_frame_callback g_FrameCallback;

char* get_savespath(void);

void new_frame();
void new_vi();

void main_set_core_defaults(void);
void main_message(m64p_msg_level level, unsigned int osd_corner, const char *format, ...);

int  main_run(void);
void main_stop(void);

void main_toggle_pause(void);
void main_advance_one(void);
void main_speedup(int percent);
void main_speeddown(int percent);
void main_draw_volume_osd(void);
void main_take_next_screenshot(void);

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
#define joyGameshark "Kbd Mapping Gameshark"

#endif /* __MAIN_H__ */

