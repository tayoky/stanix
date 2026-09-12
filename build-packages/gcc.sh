VERSION=12.2.0
SOURCE=gcc
DEPENDENCIES="tlibc-headers"
BUILD_DEPENDENCIES="autoconf automake binutils"

configure () {
	"$SOURCE_DIR/configure" --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c,c++ --enable-initfini-array --disable-multilib --enable-shared --with-pic --enable-threads=posix
}

build () {
	make all-gcc -j"$PARALLELISM"
	make all-target-libgcc -j"$PARALLELISM"
}

install () {
	make install-strip-gcc
	make install-target-libgcc
}
