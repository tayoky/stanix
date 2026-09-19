TAR="$GNU_MIRROR/gnu/gcc/gcc-$VERSION/gcc-$VERSION.tar.gz"

prepare() {
	(cd "$SOURCE_DIR/libstdc++-v3" && autoconf)
}
