VERSION="12.2.0"
SOURCE="gcc"
DEPENDENCIES="libgmp libmpfr libmpc binutils"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/gcc/"

configure () {
	gnu_configure --target="$HOST" \
		--with-sysroot=/ \
		--with-build-sysroot="$SYSROOT" \
		--disable-nls \
		--enable-languages=c,c++ \
		--enable-initfini-array \
		--disable-multilib \
		--enable-shared \
		--with-pic \
		--enable-threads=posix \
		CFLAGS="-D_Thread_local=$CFLAGS" #stupid tls workaround
}
