# helper script to build tconf packages

tconf_configure () {
	(cd "$SOURCE_DIR" && ./configure --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX" "$CONFIGURE_ARGS")
}

tconf_build () {
	make -C "$SOURCE_DIR" BUILDDIR="$BUILD_DIR"
}

tconf_install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}

configure () {
	tconf_configure
}

build () {
	tconf_build
}

install () {
	tconf_install
}
