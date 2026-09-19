SOURCE="stanix-modules"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="stanix-kernel"
. "$TOP/scripts/stanix-package.sh"

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" MODDIR="/mod"
}
