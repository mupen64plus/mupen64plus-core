#!/usr/bin/env bash
set -e +u

if [[ ${#} -ne 1 ]]; then exit 9; fi

export ENV_MSYS="$(echo "${1}" | tr [A-Z] [a-z])"
export DEPS="$(LC_ALL=C grep "${ENV_MSYS}" pkg/ldd.log | tr -s '\t' ' ' | sort | cut -d ' ' -f4 | tr '\\' '/' | tr -d ':')"

rm -f pkg/ldd.log

if [[ "${DEPS}" == "" ]]; then
	echo ":: None..."
	exit 0
fi

for LIB in ${DEPS}; do
	echo ":: Copying ${LIB##*/}"
	cp "/${LIB}" pkg/
done

exit 0
