VERSION="2.5.4"
SOURCE="libtool"
BUILD_DEPENDENCIES="tool-autoconf tool-automake"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX" --enable-shared --with-gnu-ld
}
