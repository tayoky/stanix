TAR="$GNU_MIRROR/gnu/automake/automake-$VERSION.tar.gz"
TAR_HASH="63e585246d0fc8772dffdee0724f2f988146d1a3f1c756a3dc5cfbefa3c01915"

prepare () {
	cp "$TOP/scripts/config.sub" "lib/"
}
