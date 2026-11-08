VERSION="1.15.1"
SOURCE="automake"
BUILD_DEPENDENCIES="tool-autoconf"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}
