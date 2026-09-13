VERSION="0.7.0"
SOURCE="tutils"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make -C "$SOURCE_DIR" install BUILDDIR="$BUILD_DIR" DESTDIR="$DESTDIR" BINDIR="/bin"
}
