#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - pre.mk                                                  *
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

# detect system architecture: i386, x86_64, PPC/PPC64, ALPHA, ARM, AVR32, HPPA,
# IA64, M32R, M68K, MIPS, S390, SH3, SH4, SPARC
HOST_CPU ?= $(shell uname -m)
NO_ASM ?= 1
ifneq ("$(filter x86_64 amd64,$(HOST_CPU))","")
  CPU := X86
  ifeq ("$(BITS)", "32")
    ARCH_DETECTED := 64BITS_32
  else
    ARCH_DETECTED := 64BITS
  endif
  NO_ASM := 0
  CPU_ENDIANNESS := LITTLE
endif
ifneq ("$(filter pentium i%86,$(HOST_CPU))","")
  CPU := X86
  ARCH_DETECTED := 32BITS
  NO_ASM := 0
  CPU_ENDIANNESS := LITTLE
endif
ifneq ("$(filter ppc powerpc,$(HOST_CPU))","")
  CPU := PPC
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifneq ("$(filter ppc64 powerpc64,$(HOST_CPU))","")
  CPU := PPC
  ARCH_DETECTED := 64BITS
  CPU_ENDIANNESS := BIG
endif
ifneq ("$(filter alpha%,$(HOST_CPU))","")
  CPU := ALPHA
  ARCH_DETECTED := 64BITS
  CPU_ENDIANNESS := LITTLE
endif
ifneq ("$(filter arm%b,$(HOST_CPU))","")
  CPU := ARM
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
else
ifneq ("$(filter arm%,$(HOST_CPU))","")
  CPU := ARM
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := LITTLE
endif
endif
ifneq ("$(filter hppa%b,$(HOST_CPU))","")
  CPU := HPPA
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","ia64")
  CPU := IA64
  ARCH_DETECTED := 64BITS
  CPU_ENDIANNESS := LITTLE
endif
ifeq ("$(HOST_CPU)","avr32")
  CPU := AVR32
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","m32r")
  CPU := M32R
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","m68k")
  CPU := M68K
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifneq ("$(filter mips mipseb,$(HOST_CPU))","")
  CPU := MIPS
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","mipsel")
  CPU := MIPS
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := LITTLE
endif
ifeq ("$(HOST_CPU)","s390")
  CPU := S390
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","s390x")
  CPU := S390
  ARCH_DETECTED := 64BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","sh3")
  CPU := SH3
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := LITTLE
endif
ifeq ("$(HOST_CPU)","sh3eb")
  CPU := SH3
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","sh4")
  CPU := SH4
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := LITTLE
endif
ifeq ("$(HOST_CPU)","sh4eb")
  CPU := SH4
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif
ifeq ("$(HOST_CPU)","sparc")
  CPU := SPARC
  ARCH_DETECTED := 32BITS
  CPU_ENDIANNESS := BIG
endif

# detect operation system. Currently just linux and OSX.
UNAME = $(shell uname -s)
ifeq ("$(UNAME)","Linux")
  OS = LINUX
endif
ifeq ("$(UNAME)","linux")
  OS = LINUX
endif
ifeq ("$(UNAME)","Darwin")
  OS = OSX
  LDFLAGS += -liconv -lpng
endif
ifeq ("$(UNAME)","FreeBSD")
  OS = FREEBSD
endif

ifeq ($(OS),)
   $(warning OS not supported or detected, using default linux options.)
   OS = LINUX
endif

# test for presence of SDL
ifeq ($(shell which sdl-config 2>/dev/null),)
  $(error No SDL development libraries found!)
endif

ifeq ($(OS),FREEBSD)
    SDL_FLAGS	= `${SDL_CONFIG} --cflags`
    SDL_LIBS	= `${SDL_CONFIG} --libs`
else
    SDL_FLAGS	= $(shell sdl-config --cflags)
    SDL_LIBS	= $(shell sdl-config --libs)
endif

# test for presence of FreeType
ifeq ($(shell which freetype-config 2>/dev/null),)
   $(error freetype-config not installed!)
endif
FREETYPE_LIBS	= $(shell freetype-config --libs)
FREETYPE_FLAGS	= $(shell freetype-config --cflags)

# set Freetype flags
FREETYPEINC = $(shell pkg-config --cflags freetype2)
CFLAGS += $(FREETYPEINC)

# set base program pointers and flags
ifeq ($(OS),FREEBSD)
  CC      ?= gcc
  CXX     ?= g++
  LD      ?= g++
  RM      ?= rm
  RM_F    ?= rm -f
  MV      ?= mv
  CP      ?= cp
  MD      ?= mkdir
  FIND    ?= find
  PROF    ?= gprof
  INSTALL ?= ginstall
  STRIP	  ?= strip -s
else
  CC      = gcc
  CXX     = g++
  LD      = g++
  RM      = rm
  RM_F    = rm -f
  MV      = mv
  CP      = cp
  MD      = mkdir
  FIND    = find
  PROF    = gprof
  INSTALL = ginstall
  ifeq ($(OS),LINUX)
    STRIP	= strip -s
  endif
  ifeq ($(OS),OSX)
    STRIP	= strip -x 
  endif
endif
# create SVN version defines
MUPEN_RELEASE = 1.5

ifneq ($(RELEASE),)
  MUPEN_VERSION = $(VER)
  PLUGIN_VERSION = $(VER)
else 
  ifeq ($(shell svn info ./ 2>/dev/null),)
    MUPEN_VERSION = $(MUPEN_RELEASE)-development
    PLUGIN_VERSION = $(MUPEN_RELEASE)-development
  else
    SVN_REVISION = $(shell svn info ./ 2>/dev/null | sed -n '/^Revision: /s/^Revision: //p')
    SVN_BRANCH = $(shell svn info ./ 2>/dev/null | sed -n '/^URL: /s/.*mupen64plus.//1p')
    SVN_DIFFHASH = $(shell svn diff ./ 2>/dev/null | md5sum | sed '/.*/s/ -//;/^d41d8cd98f00b204e9800998ecf8427e/d')
    MUPEN_VERSION = $(MUPEN_RELEASE)-$(SVN_BRANCH)-r$(SVN_REVISION) $(SVN_DIFFHASH)
    PLUGIN_VERSION = $(MUPEN_RELEASE)-$(SVN_BRANCH)-r$(SVN_REVISION)
  endif
endif

# set base CFLAGS and LDFLAGS
CFLAGS += -ffast-math -funroll-loops -fexpensive-optimizations -fno-strict-aliasing
ifneq ($(OS), FREEBSD)
  CFLAGS += -pipe -O3
endif

ifeq ($(OS), FREEBSD)
  CORE_LDFLAGS += -lz -lm -lpng -lfreetype
else
  CORE_LDFLAGS += -lz -lm -lpng -lfreetype -ldl
endif

# set special flags per-system
ifneq ($(OS), FREEBSD)
  ifeq ($(CPU), X86)
    ifeq ($(ARCH_DETECTED), 64BITS)
      CFLAGS += -march=athlon64
    else
      CFLAGS += -mmmx -msse -march=i686 -mtune=pentium-m
      ifneq ($(PROFILE), 1)
        CFLAGS += -fomit-frame-pointer
      endif
    endif
    # tweak flags for 32-bit build on 64-bit system
    ifeq ($(ARCH_DETECTED), 64BITS_32)
      CFLAGS += -m32
      LDFLAGS += -m32 -m elf_i386
    endif
  endif
else
  ifeq ($(ARCH_DETECTED), 64BITS_32)
    $(error Do not use the BITS=32 option with FreeBSD, use -m32 and -m elf_i386)
  endif
endif
ifeq ($(CPU), PPC)
  CFLAGS += -mcpu=powerpc
endif
ifeq ($(CPU_ENDIANNESS), BIG)
  CFLAGS += -D_BIG_ENDIAN
endif

# set CFLAGS, LIBS, and LDFLAGS according to the target OS
ifeq ($(OS),FREEBSD)
  PLUGIN_LDFLAGS = -Wl,-Bsymbolic -shared
  LIBGL_LIBS     = -L${LOCALBASE}/lib -lGL -lGLU
endif
ifeq ($(OS),LINUX)
  PLUGIN_LDFLAGS = -Wl,-Bsymbolic -shared
  LIBGL_LIBS     = -L/usr/X11R6/lib -lGL -lGLU
endif
ifeq ($(OS),OSX)
  PLUGIN_LDFLAGS = -bundle
  LIBGL_LIBS     = -framework OpenGL
  QMAKE_FLAGS    = -spec macx-g++
endif

# set flags for compile options.

# set CFLAGS macro for no assembly language if required
ifeq ($(NO_ASM), 1)
  CFLAGS += -DNO_ASM
endif

# set variables for profiling
ifeq ($(PROFILE), 1)
  CFLAGS += -pg -g
  LDFLAGS += -pg
  STRIP = true
else   # set variables for debugging symbols
  ifeq ($(DEBUG), 1)
    CFLAGS += -g
    STRIP = true
  endif
endif

SO_EXTENSION = so

