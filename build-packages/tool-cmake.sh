VERSION="4.0.0"
SOURCE="cmake"
DEPENDENCIES="tool-pkgconf"
BUILD_DEPENDENCIES="tool-autoconf tool-automake tool-libtool"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX" --parallel=="$PARALLELISM"
}

install () {
	make install DESTDIR="$DESTDIR"
}
