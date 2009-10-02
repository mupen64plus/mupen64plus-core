#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - Makefile                                                *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2007-2008 Richard Goedeken (Richard42)                  *
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

# include pre-make file with a bunch of definitions
ifeq ($(WIN32),1)
  include ../pre.mk.win32
else
  include ../pre.mk
endif

# local CFLAGS, LIBS, and LDFLAGS
CFLAGS += -fpic -DPIC

# list of object files to generate
OBJECTS = audio.o

# build targets
all: dummyaudio.$(SO_EXTENSION)

clean:
	rm -f *.o *.$(SO_EXTENSION)

# build rules
.c.o:
	$(CC) -o $@ $(CFLAGS) -c $<

dummyaudio.$(SO_EXTENSION): $(OBJECTS)
	$(CC) -shared $^ $(LDFLAGS) $(PLUGIN_LDFLAGS) -o $@
ifneq ($(OS), WINDOWS)
	$(STRIP) $@
endif

