VERSION="3.29.2"
SOURCE="cmake"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}

install () {
	make install DESTDIR="$DESTDIR"
}
