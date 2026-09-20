VERSION="0.7.0"
SOURCE="tutils"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install DESTDIR="$DESTDIR" BINDIR="/bin"
}
