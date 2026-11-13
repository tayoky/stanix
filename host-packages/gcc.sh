VERSION="12.2.0"
SOURCE="gcc"
DEPENDENCIES="libgmp libmpfr libmpc binutils zlib"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/gcc/"

configure () {
	# re enable libcody when we get netdb.h
	# TODO : enable c++ support
	gnu_configure --target="$HOST" \
		--with-sysroot=/ \
		--with-build-sysroot="$SYSROOT" \
		--with-system-zlib \
		--disable-nls \
		--enable-languages=c \
		--enable-initfini-array \
		--disable-multilib \
		--disable-libcody \
		--enable-shared \
		--with-pic \
		--enable-threads=posix \
		CFLAGS="-D_Thread_local= $CFLAGS" #stupid tls workaround
}
