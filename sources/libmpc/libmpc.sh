TAR="$GNU_MIRROR/gnu/mpc/mpc-$VERSION.tar.xz"
TAR_HASH="91204cd32f164bd3b7c992d4a6a8ce6519511aadab30f78b6982d0bf8d73e931"

prepare () {
	autoreconf -fiv
}
