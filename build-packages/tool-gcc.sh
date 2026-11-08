VERSION="12.2.0"
SOURCE="gcc"
DEPENDENCIES="tool-binutils"
BUILD_DEPENDENCIES="tlibc-headers tool-autoconf tool-automake"

configure () {
	"$SOURCE_DIR/configure" --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c,c++ --enable-initfini-array --disable-multilib --enable-shared --with-pic --enable-threads=posix
}

build () {
	make all-gcc -j"$PARALLELISM"
}

install () {
	make install-strip-gcc
}
