SOURCE="stanix-kernel"
DEPENDENCIES="tlibc limine-files"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/stanix-package.sh"

configure () {
	true
}

build () {
	true
}

install () {
	make -C "$SOURCE_DIR" install-incs DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
