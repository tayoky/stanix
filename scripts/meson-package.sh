# helper script to build meson packages
# TODO : add meson as dependency ???
BUILD_DEPENDENCIES="$BUILD_DEPENDENCIES cross-files pkgconf"

meson_configure () {
	meson setup "$SOURCE_DIR" --prefix="$PREFIX" --buildtype=release --cross-file="$BUILD_PREFIX/lib/cross-files/$HOST.meson" "$@"
}

meson_build () {
	meson build
}

meson_install () {
	meson install --destdir="$DESTDIR"
}

configure () {
	meson_configure
}

build () {
	meson_build
}

install () {
	meson_install
}
