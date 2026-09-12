VERSION="0.0.1"
SOURCE="tlibc"
BUILD_DEPENDENCIES="gcc binutils"

configure () {
	(cd "$SOURCE_DIR" && ./configure --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX"  --enable-shared)
}
