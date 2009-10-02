#***************************************************************************
#                           dummy_video Makefile
#                            -------------------
# Copyright (C) 2007-2008 Richard Goedeken (Richard42)
#***************************************************************************

#***************************************************************************
#*                                                                         *
#*   This program is free software; you can redistribute it and/or modify  *
#*   it under the terms of the GNU General Public License as published by  *
#*   the Free Software Foundation; either version 2 of the License, or     *
#*   (at your option) any later version.                                   *
#*                                                                         *
#***************************************************************************


# include pre-make file with a bunch of definitions
ifeq ($(WIN32),1)
  include ../pre.mk.win32
else
  include ../pre.mk
endif

# local CFLAGS, LIBS, and LDFLAGS
CFLAGS += -fpic -DPIC -D__LINUX__ -DX86_ASM

# list of object files to generate
OBJECTS = \
	video.o \

# build targets
all: dummyvideo.$(SO_EXTENSION)

clean:
	rm -f *.o *.$(SO_EXTENSION)

# build rules
.cpp.o:
	$(CXX) -o $@ $(CFLAGS) $(SDL_FLAGS) $(GTK_FLAGS) -c $<

dummyvideo.$(SO_EXTENSION): $(OBJECTS)
	$(CXX) -shared $^ $(LDFLAGS) $(ASM_LDFLAGS) $(PLUGIN_LDFLAGS) $(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(LIBGL_LIBS) -o $@
ifneq ($(OS), WINDOWS)
	$(STRIP) $@
endif

video.o: video.cpp
	$(CXX) $(CFLAGS) $(SDL_FLAGS) -DMAINDEF -c -o $@ $<

