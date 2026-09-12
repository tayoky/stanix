. "$TOP/host-package/stanix-base"
SOURCE="stanix-kernel"
DEPENDENCIES="tlibc"

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
