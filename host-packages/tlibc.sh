VERSION="0.0.1"
SOURCE="tlibc"
BUILD_DEPENDENCIES="gcc binutils"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

configure () {
	tconf_configure --enable-shared
}
