#!/usr/bin/env bash
set -e +u

if [[ ${#} -lt 2 ]]; then exit 9; fi

unset ARCH_ARG CC_ARG
export MAKE_PKG=0 MULTILIB=0
export ENV_ARGS="$(echo "${*}" | tr [A-Z] [a-z])"

for ARG in ${ENV_ARGS}; do
	case "${ARG}" in
		clang )
			export CC_ARG="Clang" CC="clang${C_CLANG_SUFFIX}" CXX="clang++${C_CLANG_SUFFIX}"
			;;
		gcc )
			export CC_ARG="GCC" CC="gcc${C_GCC_SUFFIX}" CXX="g++${C_GCC_SUFFIX}"
			;;
		makepkg )
			export MAKE_PKG=1
			;;
		multilib )
			export MULTILIB=1
			;;
		x64 )
			export ARCH_ARG="x64" CPU_TUNE="-march=nocona"
			;;
		x86 )
			export ARCH_ARG="x86" CPU_TUNE="-march=pentium4"
			;;
	esac
done

if [[ -z ${ARCH_ARG} ]]; then exit 8; fi
if [[ -z ${CC_ARG} ]]; then exit 7; fi

export REPO="${PWD##*/}"
if [[ "${REPO}" == "" ]]; then exit 6; fi

export BIN_OS="$(uname -s | tr [A-Z] [a-z])"
if [[ ! -z ${MSYSTEM} ]]; then export BIN_OS="msys2-$(echo "${MSYSTEM}" | tr [A-Z] [a-z])"; fi
export ENV_NAME="$(uname -s)" LDD="ldd"
export PIX="PIC" PKG_PATH="usr/local/lib/mupen64plus/"
if [[ "${REPO}" == *"core"* ]]; then
	export PKG_PATH="usr/local/lib/"
elif [[ "${REPO}" == *"ui-console"* ]]; then
	export PIX="PIE" PKG_PATH="usr/local/bin/"
fi
if [[ "${ARCH_ARG}" == "x86" ]]; then export CONFIG_OVERRIDE="BITS=32 ${CONFIG_OVERRIDE}"; fi
if [[ -z ${OPTFLAGS} ]]; then export OPTFLAGS="-O3 ${CPU_TUNE}"; fi

if [[ "${ENV_NAME}" == *"Linux"* ]]; then
	if [[ "${ARCH_ARG}" == "x86" ]]; then export CONFIG_OVERRIDE="${CONFIG_OVERRIDE} ${PIX}=1"; fi
	if [[ ${MULTILIB} -eq 0 ]]; then
		if [[ "${CC_ARG}" == "GCC" ]]; then
			if [[ "${ARCH_ARG}" == "x86" ]]; then export CC="i686-linux-gnu-${CC}" CXX="i686-linux-gnu-${CXX}"; fi
		fi
	fi
fi

if [[ "${ENV_NAME}" == *"MINGW"* ]]; then
	export INSTALL_OVERRIDE="PLUGINDIR="" SHAREDIR="" BINDIR="" MANDIR="" LIBDIR="" APPSDIR="" ICONSDIR="icons" INCDIR="api""
	export LDD="ntldd -R"
	unset PKG_PATH
fi

export G_REV="$(git rev-parse --short HEAD)"
export PKG_NAME="${REPO}-${BIN_OS}-${ARCH_ARG}-g${G_REV}"
if [[ -f "${GITHUB_ENV}" ]]; then
	set +e
	grep "PKG_NAME=${PKG_NAME}" "${GITHUB_ENV}" > /dev/null
	if [[ ${?} -ne 0 ]]; then echo "PKG_NAME=${PKG_NAME}" >> "${GITHUB_ENV}"; fi
	set -e
fi

echo ""
echo ":: CC=${CC} CXX=${CXX} ${CONFIG_OVERRIDE} OPTFLAGS=\"${OPTFLAGS}\" ::"
echo ""
${CC} --version
echo ""

make_clean () {
	make CC=${CC} CXX=${CXX} ${CONFIG_OVERRIDE} OPTFLAGS="${OPTFLAGS}" -C projects/unix clean
	echo ""
}

make_clean
make CC=${CC} CXX=${CXX} ${CONFIG_OVERRIDE} OPTFLAGS="${OPTFLAGS}" -C projects/unix all -j4
echo ""

if [[ ! -d pkg ]]; then
	mkdir pkg
	chmod -R 755 pkg
fi

pushd projects/unix > /dev/null
export ARTIFACT="$(find *mupen64plus* -type f 2> /dev/null | head -n 1)"
popd > /dev/null
if [[ "${ARTIFACT}" == "" ]]; then exit 5; fi

make CC=${CC} CXX=${CXX} ${CONFIG_OVERRIDE} OPTFLAGS="${OPTFLAGS}" -C projects/unix install ${INSTALL_OVERRIDE} DESTDIR="$(pwd)/pkg/"
echo ""
make_clean

cd pkg
if [[ -f "mupen64plus.desktop" ]]; then rm -f mupen64plus.desktop; fi
ls -gG "${PKG_PATH}${ARTIFACT}"
echo ""
${LDD} "${PKG_PATH}${ARTIFACT}" > ldd.log
cat ldd.log
echo ""
if [[ ${MAKE_PKG} -eq 1 ]]; then tar --owner=0 --group=0 --mode='og-w' -czf "${PKG_NAME}.tar.gz" usr; fi

exit 0
