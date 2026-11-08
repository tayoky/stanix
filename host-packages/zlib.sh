VERSION="1.4"
SOURCE="zlib"
WEBSITE="https://www.zlib.net/"
DEPENDENCIES="stanix-base"
BUILD_DEPENDENCIES="tool-stanix-toolchain"

configure () {
	# thanks to bananymous for the --uname
	CC="$HOST-gcc" AR="$HOST-ar" RANLIB="$HOST-ranlib" \
		"$SOURCE_DIR/configure" --prefix="$PREFIX" --uname=stanix
}
