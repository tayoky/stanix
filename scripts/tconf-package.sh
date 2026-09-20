# helper script to build tconf packages

tconf_configure () {
	"$SOURCE_DIR/configure" --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX" "$@"
}

tconf_build () {
	make -j"$PARALLELISM"
}

tconf_install () {
	make install DESTDIR="$DESTDIR"
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
