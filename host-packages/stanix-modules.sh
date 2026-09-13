. "$TOP/host-packages/stanix-base.sh"
SOURCE="stanix-modules"
BUILD_DEPENDENCIES="binutils gcc libgcc"
DEPENDENCIES="stanix-kernel"
PREFIX=/usr
. "$TOP/scripts/stanix-package.sh"

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" MODDIR="/mod"
}
