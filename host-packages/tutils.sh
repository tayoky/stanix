VERSION="0.7.0"
SOURCE="tutils"
BUILD_DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install BUILDDIR="$BUILD_DIR" DESTDIR="$DESTDIR" BINDIR="/bin"
}
