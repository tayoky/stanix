# helper script to build stanix package

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}