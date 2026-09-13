SOURCE="infones"
BUILD_DEPENDENCIES="cmake"
DEPENDENCIES="sdl12-compat"
. "$TOP/scripts/cmake-package.sh"

configure () {
	cmake_configure \
		-DSDL_INCLUDE_DIR="$SYSROOT$PREFIX/include/SDL" \
		-DSDL_LIBRARY="-lSDL"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp InfoNES "$DESTDIR$PREFIX/bin"
}
