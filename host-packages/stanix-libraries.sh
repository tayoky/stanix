SOURCE="stanix-libraries"
DEPENDENCIES="tlibc"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
. "$TOP/scripts/stanix-package.sh"

configure () {
	(cd "$SOURCE_DIR/libtgui" && ./configure --prefix="$PREFIX" --host="$HOST" --builddir="$BUILD_DIR/libtgui")
}
