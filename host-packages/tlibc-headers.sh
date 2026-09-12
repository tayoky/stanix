VERSION=0.0.1
SOURCE=tlibc

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" all-include BUILDDIR="$BUILD_DIR" TARGET="${TARGET##*-}"
}

install () {
	make -C "$SOURCE_DIR" install-include TARGET="${TARGET##*-}" BUILDDIR="$BUILD_DIR" DESTDIR="$SYSROOT"
}
