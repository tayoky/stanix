TAR="$GNU_MIRROR/gnu/mpfr/mpfr-$VERSION.tar.xz"
TAR_HASH="b67ba0383ef7e8a8563734e2e889ef5ec3c3b898a01d00fa0a6869ad81c6ce01"

prepare () {
	autoreconf -fiv
}
