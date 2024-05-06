#!/usr/bin/env bash
set -e +u

if [[ ${#} -lt 2 ]]; then exit 9; fi

unset ARCH_ARG CC_ARG
export MULTILIB=0
export ENV_ARGS="$(echo "${*}" | sed y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/)"

for ARG in ${ENV_ARGS}; do
	case "${ARG}" in
		clang )
			export CC_ARG="Clang" CC="clang" CXX="clang++"
			;;
		gcc )
			export CC_ARG="GCC" CC="gcc" CXX="g++"
			;;
		multilib )
			export MULTILIB=1
			;;
		x64 )
			export ARCH_ARG="x64" CPU_TUNE="-march=nocona" BITS=64
			;;
		x86 )
			export ARCH_ARG="x86" CPU_TUNE="-march=pentium4" BITS=32
			;;
	esac
done

if [[ -z ${ARCH_ARG} ]]; then exit 8; fi
if [[ -z ${CC_ARG} ]]; then exit 7; fi

export REPO="${PWD##*/}"
if [[ "${REPO}" == "" ]]; then exit 6; fi

export BIN_OS="$(uname -s | sed y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/)"
if [[ ! -z ${MSYSTEM} ]]; then export BIN_OS="msys2"; fi
export ENV_NAME="$(uname -s)"
export LDD="ldd"
export PKG_PATH="usr/local/lib/mupen64plus/"
if [[ "${REPO}" == *"core"* ]]; then
	export PKG_PATH="usr/local/lib/"
elif [[ "${REPO}" == *"ui-console"* ]]; then
	export PKG_PATH="usr/local/bin/"
fi

if [[ "${ENV_NAME}" == *"Linux"* ]]; then
	if [[ "${ARCH_ARG}" == "x86" ]]; then export PIC=1 PIE=1; fi
	if [[ ${MULTILIB} -eq 0 ]]; then
		if [[ "${CC_ARG}" == "GCC" ]]; then
			if [[ "${ARCH_ARG}" == "x86" ]]; then export CC="i686-linux-gnu-gcc" CXX="i686-linux-gnu-g++"; fi
		fi
	fi
fi

if [[ "${ENV_NAME}" == *"MINGW"* ]]; then
	export INSTALL_OVERRIDE="PLUGINDIR=\"\" SHAREDIR=\"\" BINDIR=\"\" MANDIR=\"\" LIBDIR=\"\" APPSDIR=\"\" ICONSDIR=\"icons\" INCDIR=\"api\""
	export LDD="ntldd"
	unset PKG_PATH
fi

export G_REV="$(git rev-parse --short HEAD)"
if [[ -f "${GITHUB_ENV}" ]]; then
	set +e
	grep "G_REV=${G_REV}" "${GITHUB_ENV}" > /dev/null
	if [[ ${?} -ne 0 ]]; then echo "G_REV=${G_REV}" >> "${GITHUB_ENV}"; fi
	set -e
fi

if [[ -z ${OPTFLAGS} ]]; then export OPTFLAGS="-O3 -flto ${CPU_TUNE}"; fi

echo ""
${CC} --version
echo ""

make CC="${CC}" CXX="${CXX}" BITS="${BITS}" ${BUILD_CONFIG} -C projects/unix clean
echo ""
make CC="${CC}" CXX="${CXX}" BITS="${BITS}" ${BUILD_CONFIG} -C projects/unix all -j4
echo ""

if [[ ! -d pkg ]]; then
	mkdir pkg
	chmod -R 755 pkg
fi
pushd projects/unix > /dev/null
export ARTIFACT="$(find *mupen64plus* -type f 2> /dev/null | head -n 1)"
popd > /dev/null

echo ""
make CC="${CC}" CXX="${CXX}" BITS="${BITS}" ${BUILD_CONFIG} -C projects/unix install ${INSTALL_OVERRIDE} DESTDIR="$(pwd)/pkg/"
echo ""

if [[ -z ${ARTIFACT} ]]; then
	exit 5
else
	cd pkg
	ls -gG "${PKG_PATH}${ARTIFACT}"
	echo ""
	${LDD} "${PKG_PATH}${ARTIFACT}" > ldd.log
	cat ldd.log
	echo ""
	if [[ "${MAKE_PKG}" == "1" ]]; then tar --owner=0 --group=0 --mode='og-w' -czf "${REPO}-${BIN_OS}-${ARCH_ARG}-g${G_REV}.tar.gz" usr; fi
fi

exit 0
