VERSION=12.2.0
SOURCE=gcc
DEPENDENCIES="tlibc"
BUILD_DEPENDENCIES="gcc"

configure () {
	true
}

build () {
	make -C "$BUILD_DIR/../gcc" all-target-libstdc++-v3 -j"$PARALLELISM"
}

install () {
	make -C "$BUILD_DIR/../gcc" install-target-strip-libstdc++-v3 DESTDIR="$DESTDIR"
}