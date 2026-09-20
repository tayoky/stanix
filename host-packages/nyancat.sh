VERSION="1.5.2"
SOURCE="nyancat"
DEPENDENCIES="stanix-base"

configure () {
    true
}

build () {
    make -C "$SOURCE_DIR" CC="$HOST-gcc"
}

install () {
    mkdir -p "$DESTDIR$PREFIX/bin"
    mkdir -p "$DESTDIR$PREFIX/share/man1"
	cp -p "$SOURCE_DIR/src/nyancat" "$DESTDIR$PREFIX/bin/"
	cp -p "$SOURCE_DIR/nyancat.1" "$DESTDIR$PREFIX/share/man1/"
}
