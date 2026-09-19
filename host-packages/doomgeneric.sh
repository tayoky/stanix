SOURCE="doomgeneric"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	# pretty weird but make must be run in a sub dir
	make -C "$SOURCE_DIR/doomgeneric" -f Makefile.stanix SYSROOT="$SYSROOT" CC="$HOST-gcc"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp "$SOURCE_DIR/doomgeneric/doom" "$DESTDIR$PREFIX/bin"
}
