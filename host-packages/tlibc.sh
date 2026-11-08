VERSION="0.0.1"
SOURCE="tlibc"
BUILD_DEPENDENCIES="tool-binutils tool-gcc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

configure () {
	tconf_configure --enable-shared --enable-libk
}
