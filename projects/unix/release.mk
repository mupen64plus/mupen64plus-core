#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - release.mk                                              *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2007-2008 Richard42                                     *
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
# Makefile for building Mupen64Plus releases

# check version number of this build
ifeq ("$(VER)", "")
  $(error Must give version parameter, ie: make VER=1.5.1)
else
  MODVER = $(shell echo "$(VER)" | sed 's/\./-/g')
endif

# create folder/zip names
SRCNAME = Mupen64Plus-$(MODVER)-src
BINNAME_32 = Mupen64Plus-$(MODVER)-bin-32
BINNAME_64 = Mupen64Plus-$(MODVER)-bin-64

# set primary build objects
ALL = src bin-32 bin-64

# build targets
targets:
	@echo "Mupen64Plus release makefile. "
	@echo "  Targets:"
	@echo "    all       == Build Mupen64Plus source zip, plus 32-bit and 64-bit binary zips"
	@echo "    src       == build Mupen64Plus source package (zip format)"
	@echo "    bin-32    == build 32-bit binary package (zip format)"
	@echo "    bin-64    == build 64-bit binary package (zip format)"
	@echo "  Options:"
	@echo "    VER=<ver> == (required) Sets version number of build (VER=1.0.3)"

all: $(ALL)

src: FORCE
	# clean objects from source tree
	$(MAKE) -f ./Makefile clean
	# remove source tree and zip file if they exist, then make empty directory for source tree
	rm -rf ../$(SRCNAME)
	mkdir ../$(SRCNAME)
	# get list of directiories in mupen64 source tree, excluding SVN stuff
	rm -f ../dirs.tmp
	find . -mindepth 1 -type d -a ! -regex '.*\.svn.*' > ../dirs.tmp
	# create the directories
	cat ../dirs.tmp | while read dirname; do relpath=$${dirname#*/}; mkdir "../$(SRCNAME)/$$relpath"; done
	rm ../dirs.tmp
	# get list of files in mupen64 source tree, excluding SVN stuff
	rm -f ../files.tmp
	find . -mindepth 1 -type f -a ! -regex '.*\.svn.*' > ../files.tmp
	# copy the files
	cat ../files.tmp | while read filename; do relpath=$${filename#*/}; cp "./$$relpath" "../$(SRCNAME)/$$relpath"; done
	rm ../files.tmp
	# delete some unnecessary stuff if present
	rm -f ../$(SRCNAME)/plugins/*.so
	rm -f ../$(SRCNAME)/plugins/*~
	rm -rf ../$(SRCNAME)/save/*
	rm -f ../$(SRCNAME)/*~
	rm -f ../$(SRCNAME)/*.cache
	# zip it up and delete the directory
	cd .. ; tar c $(SRCNAME) | gzip > $(SRCNAME).tar.gz
	rm -rf ../$(SRCNAME)

bin-32: FORCE
	$(MAKE) -f ./Makefile clean
	$(MAKE) -f ./Makefile all BITS=32 RELEASE=1 VER=$(VER)
	# remove binary tree and zip file if they exist, then make empty directory for binary tree
	rm -rf ../$(BINNAME_32)
	rm -f ../$(BINNAME_32).zip
	mkdir ../$(BINNAME_32)
	mkdir ../$(BINNAME_32)/config
	mkdir ../$(BINNAME_32)/doc
	mkdir ../$(BINNAME_32)/fonts
	mkdir ../$(BINNAME_32)/icons
	mkdir ../$(BINNAME_32)/lang
	mkdir ../$(BINNAME_32)/plugins
	mkdir ../$(BINNAME_32)/roms
	# copy files into binary structure
	cp ./mupen64plus ../$(BINNAME_32)/
	cp ./mupen64plus.ini ../$(BINNAME_32)/
	cp ./*.sh ../$(BINNAME_32)/
	cp ./*.desktop ../$(BINNAME_32)/
	cp ./README ../$(BINNAME_32)/
	cp ./RELEASE ../$(BINNAME_32)/
	cp ./LICENSES ../$(BINNAME_32)/
	cp ./config/* ../$(BINNAME_32)/config
	cp ./doc/* ../$(BINNAME_32)/doc
	cp ./fonts/* ../$(BINNAME_32)/fonts
	cp -R ./icons/* ../$(BINNAME_32)/icons
	cp ./lang/* ../$(BINNAME_32)/lang
	cp ./plugins/*.so ../$(BINNAME_32)/plugins
	cp ./roms/*.gz ../$(BINNAME_32)/roms
	# zip it up and delete the directory
	cd .. ; tar c $(BINNAME_32) | gzip > $(BINNAME_32).tar.gz
	rm -rf ../$(BINNAME_32)

bin-64: FORCE
	$(MAKE) -f ./Makefile clean
	$(MAKE) -f ./Makefile all RELEASE=1 VER=$(VER)
	# remove binary tree and zip file if they exist, then make empty directory for binary tree
	rm -rf ../$(BINNAME_64)
	rm -f ../$(BINNAME_64).zip
	mkdir ../$(BINNAME_64)
	mkdir ../$(BINNAME_64)/config
	mkdir ../$(BINNAME_64)/doc
	mkdir ../$(BINNAME_64)/fonts
	mkdir ../$(BINNAME_64)/icons
	mkdir ../$(BINNAME_64)/lang
	mkdir ../$(BINNAME_64)/plugins
	mkdir ../$(BINNAME_64)/roms
	# copy files into binary structure
	cp ./mupen64plus ../$(BINNAME_64)/
	cp ./mupen64plus.ini ../$(BINNAME_64)/
	cp ./*.sh ../$(BINNAME_64)/
	cp ./*.desktop ../$(BINNAME_64)/
	cp ./README ../$(BINNAME_64)/
	cp ./RELEASE ../$(BINNAME_64)/
	cp ./LICENSES ../$(BINNAME_64)/
	cp ./config/* ../$(BINNAME_64)/config
	cp ./doc/* ../$(BINNAME_64)/doc
	cp ./fonts/* ../$(BINNAME_64)/fonts
	cp -R ./icons/* ../$(BINNAME_64)/icons
	cp ./lang/* ../$(BINNAME_64)/lang
	cp ./plugins/*.so ../$(BINNAME_64)/plugins
	cp ./roms/*.gz ../$(BINNAME_64)/roms
	# zip it up and delete the directory
	cd .. ; tar c $(BINNAME_64) | gzip > $(BINNAME_64).tar.gz
	rm -rf ../$(BINNAME_64)

FORCE:

