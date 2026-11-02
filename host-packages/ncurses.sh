VERSION="6.5"
SOURCE="ncurses"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://invisible-island.net/ncurses"

configure() {
	gnu_configure --with-pkg-config-libdir="$PREFIX/lib/pkgconfig" \
	--enable-pc-files \
	--enable-sigwinch \
	--disable-widec \
	--without-ada \
	--without-dlsym \
	--without-cxx-binding
}

install () {
	make -C "$BUILD_DIR" install DESTDIR="$DESTDIR"
}
