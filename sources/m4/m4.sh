TAR="$GNU_MIRROR/gnu/m4/m4-$VERSION.tar.gz"
TAR_HASH="38ae59f7a30bf9c108193cc5c25fbb06014f21e230c7ede2eff614f7b7c37ed8"

prepare () {
    # TODO : run autoreconf -fiv if we ever get autopoint
    automake
}