VERSION="0.7.0"
SOURCE="tutils"
DEPENDENCIES="tlibc"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install DESTDIR="$DESTDIR" BINDIR="/bin"
}
