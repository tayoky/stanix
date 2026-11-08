SOURCE="stanix-modules"
DEPENDENCIES="stanix-kernel"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/stanix-package.sh"

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" MODDIR="/mod"
}
