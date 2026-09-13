VERSION="3.29.2"
SOURCE="cmake"
BUILD_DEPENDENCIES="pkgconf autoconf automake libtool"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}

install () {
	make install DESTDIR="$DESTDIR"
}
