#!/usr/bin/env bash
set -e +u

export REPO="${PWD##*/}"
if [[ "${REPO}" == "" ]]; then exit 9; fi

rm -fr pkg
mkdir pkg
cd binaries
for BIN in *; do
	cd "${BIN}"
	case "${BIN}" in
		*msvc* | *msys2* )
			echo ":: Creating ${BIN}.zip"
			zip -r "../../pkg/${BIN}.zip" *
			;;
		* )
			echo ":: Recovering ${BIN}.tar.gz"
			mv *.tar.gz ../../pkg/
			;;
	esac
	cd ..
done
cd ../pkg
echo ""
for ZIP in *; do
	ls -gG ${ZIP}
	tigerdeep -lz ${ZIP} >> ../${REPO}.tiger.txt
	sha256sum ${ZIP} >> ../${REPO}.sha256.txt
	sha512sum ${ZIP} >> ../${REPO}.sha512.txt
	b2sum ${ZIP} >> ../${REPO}.blake2.txt
done
mv ../${REPO}.*.txt .

echo ""
echo "TIGER:"
cat *.tiger.txt
echo ""
echo "SHA256:"
cat *.sha256.txt
echo ""
echo "SHA512:"
cat *.sha512.txt
echo ""
echo "BLAKE2:"
cat *.blake2.txt
echo ""

if [[ -f "${GITHUB_ENV}" ]]; then
	git tag -f nightly-build
	git push -f origin nightly-build
fi

exit 0
