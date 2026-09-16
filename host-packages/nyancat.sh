SOURCE="nyancat"
DEPENDENCIES="stanix-base"

configure () {
    true
}

build () {
    make -C "$SOURCE_DIR"
}

install () {
    mkdir -p "$DESTDIR$PREFIX/bin"
	cp -p "src/nyancat" "$DESTDIR$PREFIX/bin"
}
