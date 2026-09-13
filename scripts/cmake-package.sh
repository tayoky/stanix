# helper script to build cmake packages

BUILD_DEPENDENCIES="$BUILD_DEPENDENCIES cross-files cmake"
cmake_configure () {
	cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" --toolchain"$BUILD_PREFIX/lib/cross-files/$HOST.cmake" -DCMAKE_INSTALL_PREFIX="$PREFIX" "$@"
}

cmake_build () {
	cmake --build "$BUILD_DIR" --parallel="$PARALLELISM"
}

cmake_install () {
	cmake --install "$BUILD_DIR" --destdir="$DESTDIR"
}

configure () {
	cmake_configure
}

build () {
	cmake_build
}

install () {
	cmake_install
}
