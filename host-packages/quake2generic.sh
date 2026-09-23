SOURCE="quake2generic"
DEPENDENCIES="stanix-base"

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" -f Makefile-stanix -j"$PARALLELISM" BUILDDIR="$BUILD_DIR"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp "$BUILD_DIR/quake2" "$DESTDIR$PREFIX/bin"
	"$HOST-strip" "$DESTDIR$PREFIX/bin/quake2"
}
