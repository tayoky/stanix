
configure () {
	(cd "$SOURCE_DIR" && ./configure --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX" "$CONFIGURE_ARGS")
}

build () {
	make -C "$SOURCE_DIR" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
