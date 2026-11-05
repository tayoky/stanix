VERSION="12.2.0"
BUILD_DEPENDENCIES="libgcc"

PREFIX="/usr"

configure () {
    true
}

build () {
    true
}

install () {
    mkdir -p "$DESTDIR$PREFIX/lib"
    cp "$BUILD_PREFIX/$HOST/lib/libgcc_s.so" "$DESTDIR$PREFIX/lib"
    cp "$BUILD_PREFIX/$HOST/lib/libgcc_s.so.1" "$DESTDIR$PREFIX/lib"
}