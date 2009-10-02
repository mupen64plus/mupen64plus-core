/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - gui.h                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
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

#ifndef __GUI_H__
#define __GUI_H__

/* The functons which all GUIs must implement. */

/* Build the GUI and allocate necessary dependenices, but don't display
 * anything to the screen.
 */
void gui_init(int* argc, char*** argv);

/* Display the newly built GUI. */
void gui_display(void);

/* Take control of the application and wait for user input. */
void gui_main_loop(void);

typedef enum gui_message
{
    GUI_MESSAGE_INFO,
    GUI_MESSAGE_CONFIRM,
    GUI_MESSAGE_ERROR
} gui_message_t;

/* Interface for sending text messages to the GUI. messagetype can be
 * GUI_MESSAGE_INFO    - An informational message.
 * GUI_MESSAGE_CONFIRM - A yes / no confirmation dialog.
 * GUI_MESSAGE_ERROR   - An error messagee.
 * Returns true / false (1 / 0) when messagetype specifies confirm.
 * On other messagetypes return may be undefined.
 */
int gui_message(gui_message_t messagetype, const char *format, ...);

/* gui_update_rombrowser() accesses g_romcahce.length and adds upto roms to the
 * GUI's rombrowser widget. The clear flag tells the GUI whether to clear the
 * rombrowser first.
 */
void gui_update_rombrowser(unsigned int roms, unsigned short clear);

typedef enum gui_state
{
    GUI_STATE_STOPPED,
    GUI_STATE_PAUSED,
    GUI_STATE_RUNNING
} gui_state_t;

/* Allow the core to send hints to the GUI as to the state of the emulator. */
void gui_set_state(gui_state_t state);

/* TODO: Add debugger GUI APIs here. */
#ifdef DBG
void debugger_show_disassembler();
void debugger_show_registers();
void debugger_show_breakpoints();
void debugger_show_memedit();
void debugger_update_desasm();
void debugger_close();
#endif /* DBG */

#endif /* __GUI_H__ */

