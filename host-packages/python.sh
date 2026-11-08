VERSION="3.14.7"
SOURCE="python"
DEPENDENCIES="stanix-base libffi zlib libzstd"
BUILD_DEPENDENCIES="tool-python"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.python.org/"

configure () {
	gnu_configure --build="$(uname -m)-$(uname -s | tr A-Z a-z)" \
		--with-build-python="python${VERSION%.*}" \
		--with-ensurepip="install" \
		--disable-ipv6 \
		--enable-shared \
		--disable-test-modules \
		ac_cv_file__dev_ptmx=no \
		ac_cv_file__dev_ptc=no
}
