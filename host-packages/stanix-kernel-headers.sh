. "$TOP/host-packages/stanix-base.sh"
SOURCE="stanix-kernel"
BUILD_DEPENDENCIES="binutils gcc libgcc"
DEPENDENCIES="tlibc"
PREFIX=/usr
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