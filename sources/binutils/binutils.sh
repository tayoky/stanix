TAR="$GNU_MIRROR/gnu/binutils/binutils-$VERSION.tar.gz"
TAR_HASH="0cdd76777a0dfd3dd3a63f215f030208ddb91c2361d2bcc02acec0f1c16b6a2e"

prepare () {
	(cd "$SOURCE_DIR/ld" && automake)
}
