#!/bin/sh
#
# mupen64plus uninstall script
#
# Copyright 2007, 2008 The Mupen64Plus Development Team
# Modifications Copyright 2008 Guido Berhoerster
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

set -e

export PATH="/bin:/usr/bin"

usage()
{
printf "usage: $(basename $0) [PREFIX] [SHAREDIR] [BINDIR] [LIBDIR] [MANDIR] [APPLICATIONSDIR]
\tPREFIX   - installation directories prefix (default: /usr/local)
\tSHAREDIR - path to Mupen64Plus shared data files (default: \$PREFIX/share/mupen64plus)
\tBINDIR   - path to Mupen64Plus binary program files (default: \$PREFIX/bin)
\tLIBDIR   - path to Mupen64Plus plugins (default: \$SHAREDIR/plugins)
\tMANDIR   - path to manual files (default: \$PREFIX/man/man1)
\tAPPLICATIONSDIR - path to .desktop files (default: \$PREFIX/share/applications)
"
}

if [ $# -gt 6 ]; then
	usage
	exit 1
fi

PREFIX="${1:-/usr/local}"
SHAREDIR="${2:-${PREFIX}/share/mupen64plus}"
BINDIR="${3:-${PREFIX}/bin}"
LIBDIR="${4:-${SHAREDIR}/plugins}"
MANDIR="${5:-${PREFIX}/man/man1}"
APPLICATIONSDIR="${6:-${PREFIX}/share/applications}"

printf "Uninstalling Mupen64Plus from ${PREFIX}\n"
printf "Removing ${BINDIR}/mupen64plus\n"
rm -f "${BINDIR}/mupen64plus"
printf "Removing ${LIBDIR}\n"
rm -rf "${LIBDIR}"
printf "Removing ${SHAREDIR}\n"
rm -rf "${SHAREDIR}"
printf "Removing man page\n"
rm -f "${MANDIR}/mupen64plus.1.gz"
printf "Removing .desktop file\n"
rm -f "${APPLICATIONSDIR}/mupen64plus.desktop"
rmdir --ignore-fail-on-non-empty "${APPLICATIONSDIR}"
printf "Done.\n"

