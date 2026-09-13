SOURCE="stanix-libraries"
BUILD_DEPENDENCIES="binutils gcc libgcc"
DEPENDENCIES="tlibc"
. "$TOP/scripts/stanix-package.sh"

configure () {
	(cd "$SOURCE_DIR/libtgui" && ./configure --prefix="$PREFIX" --host="$HOST" --builddir="$BUILD_DIR/libtgui")
}
