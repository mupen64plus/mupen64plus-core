#!/bin/bash
#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - m64p_get.sh                                           *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2009 Richard Goedeken                                   *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

# terminate the script if any commands return a non-zero error code
set -e

echo "************************************ Creating directories"
rm -rf source
mkdir source
cd source

echo "************************************ Downloading Core library"
hg clone http://bitbucket.org/richard42/mupen64plus-core

echo "************************************ Downloading console front-end"
hg clone http://bitbucket.org/richard42/mupen64plus-ui-console

echo "************************************ Downloading SDL audio plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-audio-sdl

echo "************************************ Downloading SDL input plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-input-sdl

echo "************************************ Downloading RSP HLE plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-rsp-hle

echo "************************************ Downloading Rice Video plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-video-rice


