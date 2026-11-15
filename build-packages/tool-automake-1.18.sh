VERSION="1.18.1"
SOURCE="automake-1.18"
BUILD_DEPENDENCIES="tool-autoconf"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}
