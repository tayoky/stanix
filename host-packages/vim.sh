VERSION="9.2.1302"
SOURCE="vim"
DEPENDENCIES="ncurses"
BUILD_DEPENDENCIES="tool-stanix-toolchain"
WEBSITE="https://www.vim.org"

configure () {
	cd "$SOURCE_DIR" && ./configure --prefix="$PREFIX" --host="$HOST" --with-tlib="ncurses" \
	--disable-netbeans \
	--disable-gtktest \
	--disable-channel \
	--with-wayland=no
}

build () {
	make -C "$SOURCE_DIR" -j "$PARALLELISM"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR"
}
