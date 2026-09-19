VERSION=1.15.1
SOURCE=automake
BUILD_DEPENDENCIES="autoconf"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}
