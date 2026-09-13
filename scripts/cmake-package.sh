# helper script to build cmake packages

cmake_configure () {
	# TODO : pass toolchain
	cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" -DCMAKE_INSTALL_PREFIX="$PREFIX" "$@"
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
