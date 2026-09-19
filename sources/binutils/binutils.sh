TAR="$GNU_MIRROR/gnu/binutils/binutils-$VERSION.tar.gz"

prepare () {
	(cd "$SOURCE_DIR/ld" && automake)
}
