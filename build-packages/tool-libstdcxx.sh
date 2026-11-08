VERSION="12.2.0"
SOURCE="gcc"
DEPENDENCIES="tlibc"
BUILD_DEPENDENCIES="tool-gcc"

configure () {
	true
}

build () {
	make -C "$BUILD_DIR/../tool-gcc" all-target-libstdc++-v3 -j"$PARALLELISM"
}

install () {
	make -C "$BUILD_DIR/../tool-gcc" install-strip-target-libstdc++-v3 DESTDIR="$DESTDIR"
}
