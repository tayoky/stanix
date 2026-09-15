# helper script to build GNU packages

gnu_configure () {
	"$SOURCE_DIR/configure" --host="$HOST" --prefix="$PREFIX" "$@"
}

gnu_build () {
	make -j"$PARALLELISM"
}

gnu_install () {
	make install-strip DESTDIR="$DESTDIR"
}

configure () {
	gnu_configure
}

build () {
	gnu_build
}

install () {
	gnu_install
}
