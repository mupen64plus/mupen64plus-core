#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - Makefile                                                *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2007-2008 DarkJeztr Tillin9 Richard42                   *
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

# include output from configure script
-include ./configure.gen
#This will eventually be necessary to build
#ifndef CONFIGURE.GEN
#  $(error Run ./configure before make)
#endif

# include pre-make file with a bunch of definitions
include ./pre.mk

# set include directories required for all builds
CFLAGS += -I../../src

# Since we are building a shared library, we must compile with -fPIC for x86_64 CPUs.
# On 32-bit systems we do not want to use -fPIC because we don't have to and it has a big performance penalty on this arch
ifeq ($(ARCH_DETECTED), 64BITS)
  CFLAGS += -fPIC -DPIC -fvisibility=hidden
endif

ifeq ($(OS), FREEBSD)
  LDFLAGS += -Wl,-export-dynamic
endif
ifeq ($(OS), LINUX)
  LDFLAGS += -Wl,-export-dynamic
endif

# set options
ifeq ($(DBG), 1)
  CFLAGS += -DDBG
endif
ifeq ($(DBG_COMPARE), 1)
  CFLAGS += -DCOMPARE_CORE
endif
ifeq ($(DBG_CORE), 1)
  CFLAGS += -DCORE_DBG
endif
ifeq ($(DBG_COUNT), 1)
  CFLAGS += -DCOUNT_INSTR
endif
ifeq ($(DBG_PROFILE), 1)
  CFLAGS += -DPROFILE_R4300
endif
ifeq ($(LIRC), 1)
  CFLAGS += -DWITH_LIRC
endif

# set installation options
ifeq ($(PREFIX),)
  PREFIX := /usr/local
endif
ifeq ($(SHAREDIR),)
  SHAREDIR := $(PREFIX)/share/mupen64plus
endif
ifeq ($(BINDIR),)
  BINDIR := $(PREFIX)/bin
endif
ifeq ($(LIBDIR),)
  LIBDIR := $(SHAREDIR)/plugins
endif
ifeq ($(MANDIR),)
  MANDIR := $(PREFIX)/man/man1
endif
ifeq ($(APPLICATIONSDIR),)
  APPLICATIONSDIR := $(PREFIX)/share/applications
endif

INSTALLOPTS := $(PREFIX) $(SHAREDIR) $(BINDIR) $(LIBDIR) $(MANDIR) $(APPLICATIONSDIR)

SRCDIR = ../../src
OBJDIR = _obj

# list of object files to generate
SRC_CORE = \
	$(SRCDIR)/main/api_common.c \
	$(SRCDIR)/main/api_config.c \
	$(SRCDIR)/main/api_debugger.c \
	$(SRCDIR)/main/api_frontend.c \
	$(SRCDIR)/main/api_vidext.c \
	$(SRCDIR)/main/main.c \
	$(SRCDIR)/main/util.c \
	$(SRCDIR)/main/cheat.c \
	$(SRCDIR)/main/config.c \
	$(SRCDIR)/main/md5.c \
	$(SRCDIR)/main/rom.c \
	$(SRCDIR)/main/ini_reader.c \
	$(SRCDIR)/main/savestates.c \
	$(SRCDIR)/main/translate.c \
	$(SRCDIR)/main/adler32.c \
	$(SRCDIR)/main/zip/ioapi.c \
	$(SRCDIR)/main/zip/zip.c \
	$(SRCDIR)/main/zip/unzip.c \
	$(SRCDIR)/memory/dma.c \
	$(SRCDIR)/memory/flashram.c \
	$(SRCDIR)/memory/memory.c \
	$(SRCDIR)/memory/pif.c \
	$(SRCDIR)/memory/tlb.c \
	$(SRCDIR)/plugin/plugin.c \
	$(SRCDIR)/plugin/dummy_video.c \
	$(SRCDIR)/plugin/dummy_audio.c \
	$(SRCDIR)/plugin/dummy_input.c \
	$(SRCDIR)/r4300/r4300.c \
	$(SRCDIR)/r4300/bc.c \
	$(SRCDIR)/r4300/compare_core.c \
	$(SRCDIR)/r4300/cop0.c \
	$(SRCDIR)/r4300/cop1.c \
	$(SRCDIR)/r4300/cop1_d.c \
	$(SRCDIR)/r4300/cop1_l.c \
	$(SRCDIR)/r4300/cop1_s.c \
	$(SRCDIR)/r4300/cop1_w.c \
	$(SRCDIR)/r4300/exception.c \
	$(SRCDIR)/r4300/interupt.c \
	$(SRCDIR)/r4300/profile.c \
	$(SRCDIR)/r4300/pure_interp.c \
	$(SRCDIR)/r4300/recomp.c \
	$(SRCDIR)/r4300/special.c \
	$(SRCDIR)/r4300/regimm.c \
	$(SRCDIR)/r4300/tlb.c \
	$(SRCDIR)/osd/OGLFT.cpp \
	$(SRCDIR)/osd/osd.cpp \
	$(SRCDIR)/osd/screenshot.cpp

# handle dynamic recompiler objects
ifneq ($(NO_ASM), 1)
  ifeq ($(CPU), X86)
    ifeq ($(ARCH_DETECTED), 64BITS)
      DYNAREC = x86_64
    else
      DYNAREC = x86
    endif
  endif
endif
ifneq ($(DYNAREC), )
  SRC_DYNAREC = \
    $(SRCDIR)/r4300/$(DYNAREC)/assemble.c \
    $(SRCDIR)/r4300/$(DYNAREC)/debug.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gbc.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop0.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop1.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop1_d.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop1_l.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop1_s.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gcop1_w.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gr4300.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gregimm.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gspecial.c \
    $(SRCDIR)/r4300/$(DYNAREC)/gtlb.c \
    $(SRCDIR)/r4300/$(DYNAREC)/regcache.c \
    $(SRCDIR)/r4300/$(DYNAREC)/rjump.c
else
  SRC_DYNAREC = $(SRCDIR)/r4300/empty_dynarec.c
endif

SRC_LIRC = \
	$(SRCDIR)/main/lirc.c

SRC_DBG = \
	$(SRCDIR)/debugger/debugger.c \
	$(SRCDIR)/debugger/dbg_decoder.c \
	$(SRCDIR)/debugger/dbg_opprintf.c \
	$(SRCDIR)/debugger/dbg_memory.c \
	$(SRCDIR)/debugger/dbg_breakpoints.c

SHARE = $(shell grep CONFIG_PATH config.h | cut -d '"' -f 2)

# set primary objects and libraries for all outputs
ALL = libmupen64plus.so
SOURCE = $(SRC_CORE) $(SRC_DYNAREC)
LIBS = $(SDL_LIBS) $(LIBGL_LIBS)

# add extra objects and libraries for selected options
ifeq ($(DBG), 1)
  SOURCE +=  $(SRC_DBG)
  LIBS += -lopcodes -lbfd
endif
ifeq ($(LIRC), 1)
  SOURCE += $(SRC_LIRC)
  LDFLAGS += -llirc_client
endif

OBJECTS := $(patsubst $(SRCDIR)/%.c,   $(OBJDIR)/%.o, $(filter %.c,   $(SOURCE)))
OBJECTS += $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(filter %.cpp, $(SOURCE)))
OBJDIRS = $(dir $(OBJECTS))
$(shell mkdir -p $(OBJDIRS))

# build targets
targets:
	@echo "Mupen64Plus makefile. "
	@echo "  Targets:"
	@echo "    all           == Build Mupen64Plus core library"
	@echo "    clean         == remove object files"
	@echo "    rebuild       == clean and re-build all"
	@echo "    install       == Install Mupen64Plus core library"
	@echo "    uninstall     == Uninstall Mupen64Plus core library"
	@echo "  Options:"
	@echo "    BITS=32       == build 32-bit binaries on 64-bit machine"
	@echo "    LIRC=1        == enable LIRC support"
	@echo "    NO_ASM=1      == build without assembly (no dynamic recompiler or MMX/SSE code)"
	@echo "  Install Options:"
	@echo "    PREFIX=path   == install/uninstall prefix (default: /usr/local/)"
	@echo "    SHAREDIR=path == path to install shared data (default: PREFIX/share/mupen64plus/)"
	@echo "    BINDIR=path   == path to install mupen64plus binary (default: PREFIX/bin/)"
	@echo "    LIBDIR=path   == path to install plugin libraries (default: SHAREDIR/plugins/)"
	@echo "    MANDIR=path   == path to install manual files (default: PREFIX/man/man1/)"
	@echo "  Debugging Options:"
	@echo "    PROFILE=1     == build gprof instrumentation into binaries for profiling"
	@echo "    DBGSYM=1      == add debugging symbols to binaries"
	@echo "    DBG=1         == build graphical debugger"
	@echo "    DBG_CORE=1    == print debugging info in r4300 core"
	@echo "    DBG_COUNT=1   == print R4300 instruction count totals (64-bit dynarec only)"
	@echo "    DBG_COMPARE=1 == enable core-synchronized r4300 debugging"
	@echo "    DBG_PROFILE=1 == dump profiling data for r4300 dynarec to data file"
#	@echo "    RELEASE=1     == inhibit SVN info from version strings"
#	@echo "    VER=x.y.z     == use this version number when RELEASE=1"
# The RELEASE and VER flags are hidden from view as they should only be used internally.
# They only affect the version strings

all: $(ALL)

install:
	./install.sh $(INSTALLOPTS)

uninstall:
	./uninstall.sh $(INSTALLOPTS)

clean:
	$(RM) -rf ./_obj ./_lib libmupen64plus.so

rebuild: clean all

# build rules
libmupen64plus.so: $(MISC_DEPS) $(OBJECTS)
	$(CXX) -shared $(OBJECTS) $(LDFLAGS) $(CORE_LDFLAGS) $(LIBS) -o $@

mupen64plus.desktop: FORCE
	@sed s:SHARE_DIR:"$(SHAREDIR)": mupen64plus.desktop.in > mupen64plus.desktop

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -o $@ $(CFLAGS) $(SDL_FLAGS) -c $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -o $@ $(CFLAGS) $(SDL_FLAGS) -c $<

# This is used to force the plugin builds
FORCE:

