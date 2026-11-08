VERSION="9.2.1302"
SOURCE="vim"
DEPENDENCIES="ncurses"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
WEBSITE="https://www.vim.org"

configure () {
	cd "$SOURCE_DIR" && ./configure --prefix="$PREFIX" --host="$HOST" --with-tlib="ncurses"
}

build () {
	make -C "$SOURCE_DIR" -j "$PARALLELISM"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR"
}
