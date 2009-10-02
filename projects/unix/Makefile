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
USES_QT4 = true
USES_GTK2 = true

ifeq ($(WIN32),1)
  include ./pre.mk.win32
else
  include ./pre.mk
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
  QMAKE_CXXFLAGS = QMAKE_CXXFLAGS=-DDBG
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
ifeq ($(GUI), NONE)
  CFLAGS += -DNO_GUI
else
  ifeq ($(GUI), QT4)
  CFLAGS += $(QT_FLAGS) $(GTK_FLAGS)
  LDFLAGS += $(QT_LIBS)
    ifeq ($(DBG), 1)
      CFLAGS += $(GTK_FLAGS)
    endif
  else
    ifeq ($(GUI), GTK2)
      CFLAGS += $(GTK_FLAGS)
    endif
  endif
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

# list of object files to generate
OBJ_CORE = \
	main/main.o \
	main/romcache.o \
	main/util.o \
	main/cheat.o \
	main/config.o \
	main/md5.o \
	main/plugin.o \
	main/rom.o \
	main/ini_reader.o \
	main/savestates.o \
	main/zip/ioapi.o \
	main/zip/zip.o \
	main/zip/unzip.o \
	main/lzma/buffer.o \
	main/lzma/io.o \
	main/lzma/main.o \
	main/7zip/7zAlloc.o \
	main/7zip/7zBuffer.o \
	main/7zip/7zCrc.o \
	main/7zip/7zDecode.o \
	main/7zip/7zExtract.o \
	main/7zip/7zHeader.o \
	main/7zip/7zIn.o \
	main/7zip/7zItem.o \
	main/7zip/7zMain.o \
	main/7zip/LzmaDecode.o \
	main/7zip/BranchX86.o \
	main/7zip/BranchX86_2.o \
	memory/dma.o \
	memory/flashram.o \
	memory/memory.o \
	memory/pif.o \
	memory/tlb.o \
	r4300/r4300.o \
	r4300/bc.o \
	r4300/compare_core.o \
	r4300/cop0.o \
	r4300/cop1.o \
	r4300/cop1_d.o \
	r4300/cop1_l.o \
	r4300/cop1_s.o \
	r4300/cop1_w.o \
	r4300/exception.o \
	r4300/interupt.o \
	r4300/profile.o \
	r4300/pure_interp.o \
	r4300/recomp.o \
	r4300/special.o \
	r4300/regimm.o \
	r4300/tlb.o

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
  OBJ_DYNAREC = \
    r4300/$(DYNAREC)/assemble.o \
    r4300/$(DYNAREC)/debug.o \
    r4300/$(DYNAREC)/gbc.o \
    r4300/$(DYNAREC)/gcop0.o \
    r4300/$(DYNAREC)/gcop1.o \
    r4300/$(DYNAREC)/gcop1_d.o \
    r4300/$(DYNAREC)/gcop1_l.o \
    r4300/$(DYNAREC)/gcop1_s.o \
    r4300/$(DYNAREC)/gcop1_w.o \
    r4300/$(DYNAREC)/gr4300.o \
    r4300/$(DYNAREC)/gregimm.o \
    r4300/$(DYNAREC)/gspecial.o \
    r4300/$(DYNAREC)/gtlb.o \
    r4300/$(DYNAREC)/regcache.o \
    r4300/$(DYNAREC)/rjump.o
else
  OBJ_DYNAREC = r4300/empty_dynarec.o
endif

OBJ_LIRC = \
	main/lirc.o

OBJ_OPENGL = \
	opengl/OGLFT.o \
	opengl/osd.o \
	opengl/screenshot.o

OBJ_GTK_GUI = \
	main/gui_gtk/main_gtk.o \
        main/gui_gtk/icontheme.o \
	main/gui_gtk/aboutdialog.o \
	main/gui_gtk/cheatdialog.o \
	main/gui_gtk/configdialog.o \
	main/gui_gtk/rombrowser.o \
	main/gui_gtk/romproperties.o

OBJ_DBG = \
	debugger/debugger.o \
	debugger/decoder.o \
	debugger/opprintf.o \
	debugger/memory.o \
	debugger/breakpoints.o

OBJ_GTK_DBG_GUI = \
	main/gui_gtk/debugger/debugger.o \
	main/gui_gtk/debugger/breakpoints.o \
	main/gui_gtk/debugger/desasm.o \
	main/gui_gtk/debugger/memedit.o \
	main/gui_gtk/debugger/varlist.o \
	main/gui_gtk/debugger/registers.o \
	main/gui_gtk/debugger/regGPR.o \
	main/gui_gtk/debugger/regCop0.o \
	main/gui_gtk/debugger/regSpecial.o \
	main/gui_gtk/debugger/regCop1.o \
	main/gui_gtk/debugger/regAI.o \
	main/gui_gtk/debugger/regPI.o \
	main/gui_gtk/debugger/regRI.o \
	main/gui_gtk/debugger/regSI.o \
	main/gui_gtk/debugger/regVI.o \
	main/gui_gtk/debugger/regTLB.o \
	main/gui_gtk/debugger/ui_clist_edit.o \
	main/gui_gtk/debugger/ui_disasm_list.o

OBJ_QT4_GUI = main/gui_qt4/libgui_qt4.a
OBJ_QT4_DBG_GUI = main/gui_qt4/debugger/libguidbg_qt4.a
QT4_GUI_DEPENDENCIES = main/gui_qt4/Makefile
QMAKE_CONFIG = CONFIG+=release

PLUGINS	= 

ifneq ($(NODUMMY), 1)
PLUGINS += plugins/dummyaudio.$(SO_EXTENSION) \
           plugins/dummyvideo.$(SO_EXTENSION) \
           plugins/dummyinput.$(SO_EXTENSION)
endif

ifneq ($(NOGLN64), 1)
PLUGINS += plugins/glN64.$(SO_EXTENSION)
endif

ifneq ($(NORICE), 1)
PLUGINS += plugins/ricevideo.$(SO_EXTENSION)
endif

ifneq ($(NOGLIDE), 1)
PLUGINS += plugins/glide64.$(SO_EXTENSION)
endif

ifneq ($(NOHLERSP), 1)
PLUGINS += plugins/mupen64_hle_rsp_azimer.$(SO_EXTENSION)
endif

ifneq ($(NOMINPUT), 1)
PLUGINS += plugins/mupen64_input.$(SO_EXTENSION)
endif

ifneq ($(NOJTTL), 1)
PLUGINS +=plugins/jttl_audio.$(SO_EXTENSION)
endif

ifneq ($(NOBLIGHT), 1)
PLUGINS	+=plugins/blight_input.$(SO_EXTENSION)
endif

ifeq ($(Z64), 1)
PLUGINS +=plugins/z64-rsp.$(SO_EXTENSION) \
          plugins/z64gl.$(SO_EXTENSION)
endif

SHARE = $(shell grep CONFIG_PATH config.h | cut -d '"' -f 2)

# set primary objects and libraries for all outputs
ALL = mupen64plus $(PLUGINS)
OBJECTS = $(OBJ_CORE) $(OBJ_DYNAREC) $(OBJ_OPENGL)
LIBS = $(SDL_LIBS) $(LIBGL_LIBS) -lbz2

# add extra objects and libraries for selected options
ifneq ($(GUI), NONE)
  ifneq ($(OS), WINDOWS)
    MISC_DEPS = mupen64plus.desktop
  endif
endif
ifeq ($(GUI), QT4)
  OBJECTS += $(OBJ_QT4_GUI)
  LIBS += $(QT_LIBS) $(GTK_LIBS)
else
  # we reimplement the translation functions in the Qt gui
  OBJECTS += main/translate.o
  ifneq ($(GUI), NONE)
    OBJECTS += $(OBJ_GTK_GUI)
    LIBS += $(GTK_LIBS) $(GTHREAD_LIBS)
  endif
endif
ifeq ($(DBG), 1)
  OBJECTS +=  $(OBJ_DBG)
  LIBS += -lopcodes -lbfd
  ifeq ($(GUI), GTK2)
    OBJECTS += $(OBJ_GTK_DBG_GUI)
  endif
  ifeq ($(GUI), QT4)
    OBJECTS += $(OBJ_QT4_DBG_GUI)
    QT4_GUI_DEPENDENCIES += main/gui_qt4/debugger/libguidbg_qt4.a
    QT4_CONFIG = CONFIG+=debug
  endif
endif
ifeq ($(LIRC), 1)
  OBJECTS += $(OBJ_LIRC)
  LDFLAGS += -llirc_client
endif

# build targets
targets:
	@echo "Mupen64Plus makefile. "
	@echo "  Targets:"
	@echo "    all           == Build Mupen64Plus and all plugins"
	@echo "    clean         == remove object files (also try clean-core or clean-plugins)"
	@echo "    rebuild       == clean and re-build all"
	@echo "    install       == Install Mupen64Plus and all plugins"
	@echo "    uninstall     == Uninstall Mupen64Plus and all plugins"
	@echo "  Options:"
	@echo "    BITS=32       == build 32-bit binaries on 64-bit machine"
	@echo "    LIRC=1        == enable LIRC support"
	@echo "    NO_RESAMP=1   == disable libsamplerate support in jttl_audio"
	@echo "    NO_ASM=1      == build without assembly (no dynamic recompiler or MMX/SSE code)"
	@echo "    GUI=NONE      == build without GUI support"
	@echo "    GUI=GTK2      == build with GTK2 GUI support (default)"
	@echo "    GUI=QT4       == build with QT4 GUI support"
	@echo "    WIN32=1       == mingw build"
	@echo "    Z64=1         == include z64 rsp plugin"
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

all: version.h $(ALL)

mupen64plus: $(MISC_DEPS) version.h $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(CORE_LDFLAGS) $(LIBS) -o $@
ifneq ($(OS), WINDOWS)
	$(STRIP) $@
endif

install:
	./install.sh $(INSTALLOPTS)

uninstall:
	./uninstall.sh $(INSTALLOPTS)

clean-plugins:
ifneq ($(OS), WINDOWS)
	$(MAKE) -C blight_input clean
	$(MAKE) -C dummy_audio clean
	$(MAKE) -C dummy_video clean
	$(MAKE) -C dummy_input clean
	$(MAKE) -C glN64 clean
	$(MAKE) -C rice_video clean
	$(MAKE) -C glide64 clean
	$(MAKE) -C jttl_audio clean
	$(MAKE) -C rsp_hle clean
	$(MAKE) -C mupen64_input clean
	$(MAKE) -C z64 clean
	$(RM_F) plugins/mupen64_input.$(SO_EXTENSION) blight_input/arial.ttf.c blight_input/ttftoh plugins/blight_input.$(SO_EXTENSION) plugins/mupen64_hle_rsp_azimer.$(SO_EXTENSION)
	$(RM_F) plugins/dummyaudio.$(SO_EXTENSION) plugins/dummyvideo.$(SO_EXTENSION) plugins/jttl_audio.$(SO_EXTENSION) plugins/glN64.$(SO_EXTENSION) plugins/ricevideo.$(SO_EXTENSION) plugins/glide64.$(SO_EXTENSION)
	$(RM_F) plugins/dummyinput.$(SO_EXTENSION) plugins/z64-rsp.$(SO_EXTENSION) plugins/z64gl.$(SO_EXTENSION)
endif

clean-core:
ifneq ($(OS), WINDOWS)
	$(RM_F) ./r4300/*.o ./r4300/x86/*.o ./r4300/x86_64/*.o ./memory/*.o ./debugger/*.o ./opengl/*.o
	$(RM_F) ./main/*.o ./main/version.h ./main/zip/*.o ./main/lzma/*.o ./main/7zip/*.o ./main/gui_gtk/*.o ./main/gui_gtk/debugger/*.o
	$(RM_F) mupen64plus mupen64plus.desktop
	$(RM_F) main/gui_qt4/moc_* main/gui_qt4/ui_*.h main/gui_qt4/*.o main/gui_qt4/*.a main/gui_qt4/Makefile
	$(RM_F) main/gui_qt4/debugger/moc_* main/gui_qt4/debugger/ui_*.h main/gui_qt4/debugger/*.o main/gui_qt4/debugger/*.a main/gui_qt4/debugger/Makefile
	$(RM_F) translations/*.qm
else
	del /S *.o *.$(SO_EXTENSION) mupen64plus.exe moc_* *.a *.qm
	cd main\gui_qt4
	del /S ui_*.h
endif

clean: clean-core clean-plugins

rebuild: clean all

# build rules
mupen64plus.desktop: FORCE
	@sed s:SHARE_DIR:"$(SHAREDIR)": mupen64plus.desktop.in > mupen64plus.desktop

version.h: FORCE
ifneq ($(OS), WINDOWS)
	@sed 's|@MUPEN_VERSION@|\"$(MUPEN_VERSION)\"| ; s|@PLUGIN_VERSION@|\"$(PLUGIN_VERSION)\"|' \
        main/version.template > version.h
	@$(MV) version.h main/version.h
else
	copy version.win32.h main\version.h
endif

.cpp.o:
	$(CXX) -o $@ $(CFLAGS) $(SDL_FLAGS) -c $<

.c.o:
	$(CC) -o $@ $(CFLAGS) $(SDL_FLAGS) -c $<

main/gui_qt4/debugger/Makefile: FORCE
	${QMAKE} main/gui_qt4/debugger/guidbg_qt4.pro ${QMAKE_FLAGS} ${QT4_CONFIG} ${QMAKE_CXXFLAGS} -o main/gui_qt4/debugger/Makefile

main/gui_qt4/debugger/libguidbg_qt4.a: main/gui_qt4/debugger/Makefile FORCE
ifneq ($(OS), WINDOWS)
	${MAKE} -C main/gui_qt4/debugger
# Run lrelease only on ts files with locale suffix, makes no sense to run it on
# the template. For some reason this fails on windows.
#	${LRELEASE} translations/*_*.ts
else
# I wonder whether we can avoid this somehow
	${MAKE} -C main/gui_qt4/debugger CXXFLAGS="${CFLAGS}"
	copy main\gui_qt4\debugger\release\libgui_qt4.a main\gui_qt4\debugger
endif

main/gui_qt4/Makefile: FORCE
	${QMAKE} main/gui_qt4/gui_qt4.pro ${QMAKE_FLAGS} ${QT4_CONFIG} ${QMAKE_CXXFLAGS} -o main/gui_qt4/Makefile

main/gui_qt4/libgui_qt4.a: ${QT4_GUI_DEPENDENCIES} FORCE
ifneq ($(OS), WINDOWS)
	${MAKE} -C main/gui_qt4
# Run lrelease only on ts files with locale suffix, makes no sense to run it on
# the template. For some reason this fails on windows.
	${LRELEASE} translations/*_*.ts
else
# I wonder whether we can avoid this somehow
	${MAKE} -C main/gui_qt4 CXXFLAGS="${CFLAGS}"
	copy main\gui_qt4\release\libgui_qt4.a main\gui_qt4
endif

plugins/blight_input.$(SO_EXTENSION): FORCE
	$(MAKE) -C blight_input all
ifneq ($(OS), WINDOWS)
	@$(CP) ./blight_input/blight_input.so ./plugins/blight_input.so
else
	copy blight_input\blight_input.dll plugins
endif

plugins/dummyaudio.$(SO_EXTENSION): FORCE
	$(MAKE) -C dummy_audio all
ifneq ($(OS), WINDOWS)
	@$(CP) ./dummy_audio/dummyaudio.so ./plugins/dummyaudio.so
else
	copy dummy_audio\dummyaudio.dll plugins
endif

plugins/dummyvideo.$(SO_EXTENSION): FORCE
	$(MAKE) -C dummy_video all
ifneq ($(OS), WINDOWS)
	@$(CP) ./dummy_video/dummyvideo.so ./plugins/dummyvideo.so
else
	copy dummy_video\dummyvideo.dll plugins
endif

plugins/dummyinput.$(SO_EXTENSION): FORCE
	$(MAKE) -C dummy_input all
	@$(CP) ./dummy_input/dummyinput.$(SO_EXTENSION) ./plugins/dummyinput.$(SO_EXTENSION)

plugins/glN64.$(SO_EXTENSION): FORCE
	$(MAKE) -C glN64 all
	@$(CP) ./glN64/glN64.$(SO_EXTENSION) ./plugins/glN64.$(SO_EXTENSION)

plugins/ricevideo.$(SO_EXTENSION): FORCE
	$(MAKE) -C rice_video all
	@$(CP) ./rice_video/ricevideo.$(SO_EXTENSION) ./plugins/ricevideo.$(SO_EXTENSION)

plugins/glide64.$(SO_EXTENSION): FORCE
	$(MAKE) -C glide64 all
	@$(CP) ./glide64/glide64.$(SO_EXTENSION) ./plugins/glide64.$(SO_EXTENSION)

plugins/jttl_audio.$(SO_EXTENSION): FORCE
	$(MAKE) -C jttl_audio all
	@$(CP) ./jttl_audio/jttl_audio.$(SO_EXTENSION) ./plugins/jttl_audio.$(SO_EXTENSION)

plugins/mupen64_hle_rsp_azimer.$(SO_EXTENSION): FORCE
	$(MAKE) -C rsp_hle all
	@$(CP) ./rsp_hle/mupen64_hle_rsp_azimer.$(SO_EXTENSION) ./plugins/mupen64_hle_rsp_azimer.$(SO_EXTENSION)

plugins/mupen64_input.$(SO_EXTENSION): FORCE
	$(MAKE) -C mupen64_input all
	@$(CP) ./mupen64_input/mupen64_input.$(SO_EXTENSION) ./plugins/mupen64_input.$(SO_EXTENSION)

plugins/z64gl.$(SO_EXTENSION): FORCE
	$(MAKE) -C z64 z64gl.$(SO_EXTENSION)
	@$(CP) ./z64/z64gl.$(SO_EXTENSION) ./plugins/z64gl.$(SO_EXTENSION)

plugins/z64-rsp.$(SO_EXTENSION): FORCE
	$(MAKE) -C z64 z64-rsp.$(SO_EXTENSION)
	@$(CP) ./z64/z64-rsp.$(SO_EXTENSION) ./plugins/z64-rsp.$(SO_EXTENSION)

# This is used to force the plugin builds
FORCE:

