VERSION="1.2.76"
SOURCE="sdl12-compat"
WEBSITE="https://libsdl.org/"
DEPENDENCIES="sdl2-compat"

configure () {
	cmake_configure -DSDL12TESTS=OFF
}

install () {
	cmake_install

	# symlink sdl.pc so old programs can find it
	ln -sf "sdl12_compat.pc" "$DESTDIR$PREFIX/lib/pkgconfig/sdl.pc"
}
