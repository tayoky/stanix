SOURCE="infones"
BUILD_DEPENDENCIES="cmake"
DEPENDENCIES="sdl12-compat"

configure () {
	cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" --toolchain="$CMAKE_CROSS" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
	-DSDL_INCLUDE_DIR="$SYSROOT$PREFIX/include/SDL" \
	-DSDL_LIBRARY="-lSDL"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp InfoNES "$DESTDIR$PREFIX/bin"
}
