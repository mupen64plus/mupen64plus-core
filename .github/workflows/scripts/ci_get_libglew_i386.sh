#!/usr/bin/env bash
set -e +u

# NOTE: There is no native "libglew-dev:i386" in Ubuntu 24.04, we will use Debian ones...

export GLEWVER="$(apt list libglew-dev | grep "amd64" | cut -d ' ' -f2)"
export PKGS="$(apt list libglew* | grep "${GLEWVER}" | cut -d '/' -f1)"
export DEBSOURCE="http://http.us.debian.org/debian/pool/main/g/glew/"
export BUILDP1="$(echo "${GLEWVER}" | cut -d '-' -f1)"
export BUILDP2="$(echo "${GLEWVER}" | cut -d '-' -f2)"
export GLEWBUILD="${BUILDP1}"
if [[ "${GLEWBUILD}" != "${GLEWVER}" ]]; then export GLEWBUILD="${BUILDP1}-${BUILDP2:0:1}"; fi
export PKGVER_LS="$(curl -sS ${DEBSOURCE} | grep "${GLEWBUILD}" | grep "amd64" | cut -d '_' -f2 | sort)"
if [[ "${PKGVER_LS}" != "" ]]; then
	for VER in ${PKGVER_LS}; do export PKGVER="${VER}"; done
	cd /tmp
	for PKG in ${PKGS}; do
		for ARCH in amd64 i386; do curl -L -O "${DEBSOURCE}${PKG}_${PKGVER}_${ARCH}.deb"; done
	done
	sudo dpkg -i libglew*${PKGVER}*.deb
else
	exit 9
fi

sudo ldconfig

exit 0
