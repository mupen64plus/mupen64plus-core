#!/usr/bin/env bash
set -e +u

if [[ ${#} -lt 2 ]]; then exit 9; fi

unset ARCH_DEP CC_DEP
export BUILD_DEPS_I386="crossbuild-essential-i386 g++${C_GCC_SUFFIX}-i686-linux-gnu gcc${C_GCC_SUFFIX}-i686-linux-gnu libc6-i386"
export HOTFIX_I386="libatomic1:i386 libgcc-s1:i386 libstdc++6:i386 ${HOTFIX_I386}"
export ENV_ARGS="$(echo "${*}" | tr [A-Z] [a-z])"

for ARG in ${ENV_ARGS}; do
	case "${ARG}" in
		clang )
			export CC_DEP="clang${C_CLANG_SUFFIX}"
			;;
		gcc )
			export CC_DEP="g++${C_GCC_SUFFIX} gcc${C_GCC_SUFFIX}"
			;;
		multilib )
			export BUILD_DEPS_I386="g++${C_GCC_SUFFIX}-multilib gcc${C_GCC_SUFFIX}-multilib libc6-dev-i386"
			;;
		x64 )
			export ARCH_DEP="x64"
			;;
		x86 )
			export ARCH_DEP="x86"
			;;
	esac
done

if [[ -z ${ARCH_DEP} ]]; then exit 8; fi
if [[ -z ${CC_DEP} ]]; then exit 7; fi

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
if [[ "${ARCH_DEP}" == "x86" ]]; then sudo dpkg --add-architecture i386; fi
sudo apt-get update
sudo apt-get -y install build-essential git nasm pkg-config ${CC_DEP} ${BUILD_DEPS}
if [[ "${ARCH_DEP}" == "x86" ]]; then
	if [[ "${BUILD_DEPS}" != "" ]]; then
		for DEP in ${BUILD_DEPS}; do
			if [[ "${DEP}" != "libglew-dev" ]]; then export BUILD_DEPS_I386="${BUILD_DEPS_I386} ${DEP}:i386"; fi
		done
	fi
	sudo apt-get --reinstall -y install ${BUILD_DEPS_I386} ${HOTFIX_I386}
fi
sudo ldconfig

exit 0
