#!/usr/bin/env bash
set -e +u

if [[ ${#} -ne 1 ]]; then exit 9; fi

export ENV_MSYS="$(echo "${1}" | sed y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/)"
export DEPS="$(LC_ALL=C grep "${ENV_MSYS}" "pkg/ldd.log" | sort | cut -d ' ' -f1)"

if [[ "${DEPS}" == "" ]]; then exit 0; fi

for LIB in ${DEPS}; do
	echo ":: Copying ${LIB}"
	cp "/${ENV_MSYS}/bin/${LIB}" pkg/
done

rm -f pkg/ldd.log

exit 0
