# helper script to build meson packages
# TODO : add meson as dependency ???
BUILD_DEPENDENCIES="$BUILD_DEPENDENCIES tool-cross-files tool-pkgconf tool-meson"

meson_configure () {
	meson setup "$SOURCE_DIR" --reconfigure --prefix="$PREFIX" --buildtype=release --cross-file="$BUILD_PREFIX/tool-cross-files/lib/cross-files/$HOST.meson" "$@"
}

meson_build () {
	meson compile
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
