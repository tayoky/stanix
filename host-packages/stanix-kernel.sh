SOURCE="stanix-kernel"
BUILD_DEPENDENCIES="binutils gcc libgcc"
DEPENDENCIES="tlibc stanix-kernel-headers stanix-libraries"
. "$TOP/scripts/stanix-package.sh"

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" BINDIR="/boot"
}
