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

echo "creating directories"
rm -rf source test
mkdir source
mkdir -p test
cd source

echo "downloading and building core library"
hg clone http://bitbucket.org/richard42/mupen64plus-core
make -C mupen64plus-core/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-core/projects/unix/libmupen64plus.so ../test
cp mupen64plus-core/data/mupen64plus.ini ../test/
cp mupen64plus-core/data/font.ttf ../test/
gunzip --stdout mupen64plus-core/roms/mupen64plus.v64.gz > ../test/m64p_test_rom.v64

echo "downloading and building console front-end"
hg clone http://bitbucket.org/richard42/mupen64plus-ui-console
make -C mupen64plus-ui-console/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-ui-console/projects/unix/mupen64plus-cli ../test

echo "downloading and building audio plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-audio-sdl
make -C mupen64plus-audio-sdl/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-audio-sdl/projects/unix/mupen64plus-audio-sdl.so ../test

echo "downloading and building input plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-input-sdl
make -C mupen64plus-input-sdl/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-input-sdl/projects/unix/mupen64plus-input-sdl.so ../test

echo "downloading and building RSP plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-rsp-hle
make -C mupen64plus-rsp-hle/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-rsp-hle/projects/unix/mupen64plus-rsp-hle.so ../test

echo "downloading and building video plugin"
hg clone http://bitbucket.org/richard42/mupen64plus-video-rice
make -C mupen64plus-video-rice/projects/unix all $@
if [ $? -ne 0 ]; then
  exit $?
fi
mv mupen64plus-video-rice/projects/unix/mupen64plus-video-rice.so ../test
cp mupen64plus-video-rice/data/RiceVideoLinux.ini ../test/

cd ../test
./mupen64plus-cli ./m64p_test_rom.v64

