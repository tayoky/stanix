VERSION=2.44
SOURCE=binutils
WEBSITE=https://www.gnu.org/software/binutils/
DEPENDENCIES="base"

configure() {
	"$SOURCE_DIR/configure" --host="$HOST" \
	--target="$HOST" \
	--prefix="$PREFIX" \
	--with-sysroot=/ \
	--with-build-sysroot=$SYSROOT \
	--disable-nls --disable-werror \
	--enable-shared \
	CFLAGS="-D_Thread_local=$CFLAGS" #stupid tls workaround
}

install() {
	make install-strip DESTDIR="$DESTDIR"
}
