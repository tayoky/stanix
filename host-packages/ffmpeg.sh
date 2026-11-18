VERSION="8.1.2"
SOURCE="ffmpeg"
WEBSITE="https://www.ffmpeg.org/"
DEPENDENCIES="sdl2-compat zlib"
BUILD_DEPENDENCIES="tool-stanix-toolchain tool-pkgconf"

configure () {
	# ffmpeg libraries are quite big
	# so we disable static libraries for the moment
	"$SOURCE_DIR/configure" --prefix="$PREFIX" \
	--target-os="none" --arch="${HOST%%-*}" \
	--enable-cross-compile \
	--cross-prefix="$HOST-" \
	--enable-shared \
	--disable-static \
	--enable-gpl \
	--enable-version3 \
	--disable-openssl
}
