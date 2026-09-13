VERSION="0.1.0"
SOURCE="tash"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make -C "$SOURCE_DIR" install BUILDDIR="$BUILD_DIR" DESTDIR="$DESTDIR" BINDIR="/bin"
}
