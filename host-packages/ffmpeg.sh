VERSION=8.1.2
SOURCE="ffmpeg"
WEBSITE="https://www.ffmpeg.org/"
DEPENDENCIES="sdl2-compat zlib"

configure () {
	# TODO : remove --disable-asm when we get posix_memalign
	"$SOURCE_DIR/configure" --prefix="$PREFIX" \
	--target-os="none" --arch="${HOST%%-*}" \
	--disable-asm --disable-inline-asm \
	--cc="$HOST-gcc" --cxx="$HOST-g++" \
	--enable-cross-compile \
	--enable-shared \
	--enable-gpl \
	--enable-version3 \
	--disable-openssl
}
