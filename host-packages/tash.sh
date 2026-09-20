VERSION="0.1.0"
SOURCE="tash"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install DESTDIR="$DESTDIR" BINDIR="/bin"
}
