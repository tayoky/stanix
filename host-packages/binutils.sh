VERSION=2.44
SOURCE=binutils
WEBSITE=https://www.gnu.org/software/binutils/
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"

configure () {
	gnu_configure --target="$HOST" \
	--with-sysroot=/ \
	--with-build-sysroot=$SYSROOT \
	--disable-nls --disable-werror \
	--enable-shared \
	CFLAGS="-D_Thread_local=$CFLAGS" #stupid tls workaround
}
