#***************************************************************************
#                           dummy_input Makefile
#                            -------------------
# Copyright (C) 2007-2008 Scott Gorman (okaygo)
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
include ../pre.mk

# local CFLAGS, LIBS, and LDFLAGS
CFLAGS += -fpic -DPIC -D__LINUX__ -DX86_ASM

# list of object files to generate
OBJECTS = \
	input.o \

# build targets
all: dummyinput.so

clean:
	rm -f *.o *.so

# build rules
.c.o:
	$(CXX) -o $@ $(CFLAGS) $(SDL_FLAGS) $(GTK_FLAGS) -c $<

dummyinput.so: $(OBJECTS)
	$(CXX) $^ $(LDFLAGS) $(ASM_LDFLAGS) $(PLUGIN_LDFLAGS) $(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(LIBGL_LIBS) -o $@
	$(STRIP) $@

input.o: input.c
	$(CXX) $(CFLAGS) $(SDL_FLAGS) -DMAINDEF -c -o $@ $<

