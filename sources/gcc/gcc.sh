TAR="$GNU_MIRROR/gnu/gcc/gcc-$VERSION/gcc-$VERSION.tar.gz"
TAR_HASH="ac6b317eb4d25444d87cf29c0d141dedc1323a1833ec9995211b13e1a851261c"

prepare() {
	(cd "$SOURCE_DIR/libstdc++-v3" && autoconf)
}
