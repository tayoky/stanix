SOURCE="stanix-kernel"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc limine-files"
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
