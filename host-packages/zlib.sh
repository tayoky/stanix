VERSION="1.3.2"
SOURCE="zlib"
WEBSITE="https://www.zlib.net/"
DEPENDENCIES="stanix-base"

configure () {
	#thanks to bananymous for the --uname
	CC="$HOST-gcc" AR="$HOST-ar" RANLIB="$HOST-ranlib" \
		"$SOURCE_DIR/configure" --prefix="$PREFIX" --uname=stanix
}
