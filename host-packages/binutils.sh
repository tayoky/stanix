VERSION="2.44"
SOURCE="binutils"
DEPENDENCIES="stanix-base"
BUILD_DEPENDENCIES="tool-autoconf tool-automake tool-stanix-toolchain"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/binutils/"

configure () {
	gnu_configure --target="$HOST" \
	--with-sysroot=/ \
	--with-build-sysroot="$SYSROOT" \
	--disable-nls --disable-werror \
	--enable-shared \
	CFLAGS="-D_Thread_local= $CFLAGS" # stupid tls workaround
}
