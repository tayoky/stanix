SOURCE="quake2generic"
DEPENDENCIES="stanix-base"

build () {
	make -C "$SOURCE_DIR" -f Makefile-stanix -j"$PARALLELISM" BUILDDIR="$BUILD_DIR"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp "$BUILD_DIR/quake2" "$DESTDIR$PREFIX/bin"
}
