SOURCE="stanix-kernel"
DEPENDENCIES="tlibc stanix-kernel-headers stanix-libraries"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/stanix-package.sh"

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" BUILDDIR="$BUILD_DIR"
}

install () {
	# install only the kernel binary
	make -C "$SOURCE_DIR" install-bin CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" BINDIR="/boot"
}
