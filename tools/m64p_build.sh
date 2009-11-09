#!/bin/bash
#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - m64p_build.sh                                           *
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

mkdir -p test

echo "************************************ Building core library"
make -C source/mupen64plus-core/projects/unix clean
make -C source/mupen64plus-core/projects/unix all $@
mv source/mupen64plus-core/projects/unix/*.so ./test
cp source/mupen64plus-core/data/mupen64plus.ini ./test/
cp source/mupen64plus-core/data/font.ttf ./test/
gunzip --stdout source/mupen64plus-core/roms/mupen64plus.v64.gz > ./test/m64p_test_rom.v64

echo "************************************ Building console front-end"
make -C source/mupen64plus-ui-console/projects/unix clean
make -C source/mupen64plus-ui-console/projects/unix all $@
mv source/mupen64plus-ui-console/projects/unix/mupen64plus-cli ./test

echo "************************************ Building audio plugin"
make -C source/mupen64plus-audio-sdl/projects/unix clean
make -C source/mupen64plus-audio-sdl/projects/unix all $@
mv source/mupen64plus-audio-sdl/projects/unix/mupen64plus-audio-sdl.so ./test

echo "************************************ Building input plugin"
make -C source/mupen64plus-input-sdl/projects/unix clean
make -C source/mupen64plus-input-sdl/projects/unix all $@
mv source/mupen64plus-input-sdl/projects/unix/mupen64plus-input-sdl.so ./test

echo "************************************ Building RSP plugin"
make -C source/mupen64plus-rsp-hle/projects/unix clean
make -C source/mupen64plus-rsp-hle/projects/unix all $@
mv source/mupen64plus-rsp-hle/projects/unix/mupen64plus-rsp-hle.so ./test

echo "************************************ Building video plugin"
make -C source/mupen64plus-video-rice/projects/unix clean
make -C source/mupen64plus-video-rice/projects/unix all $@
mv source/mupen64plus-video-rice/projects/unix/mupen64plus-video-rice.so ./test
cp source/mupen64plus-video-rice/data/RiceVideoLinux.ini ./test/


