VERSION="12.2.0"
BUILD_DEPENDENCIES="libstdcxx"

configure () {
    true
}

build () {
    true
}

install () {
    mkdir -p "$DESTDIR$PREFIX/lib"
    LIBSTDCXXNAME="$(readlink $BUILD_PREFIX/$HOST/lib/libstdc++.so)"
    cp "$BUILD_PREFIX/$HOST/lib/$LIBSTDCXXNAME" "$DESTDIR$PREFIX/lib"
    ln -sf "$LIBSTDCXXNAME" "$DESTDIR$PREFIX/lib/${LIBSTDCXXNAME%.*.*}"
    ln -sf "$LIBSTDCXXNAME" "$DESTDIR$PREFIX/lib/libstdc++.so"
}