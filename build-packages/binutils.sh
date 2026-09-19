VERSION=2.44
SOURCE=binutils
BUILD_DEPENDENCIES="autoconf automake"

configure () {
	"$SOURCE_DIR"/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --disable-werror --enable-shared
}
