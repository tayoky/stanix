VERSION=0.0.1
SOURCE=tlibc

# install first party packages in /usr
PREFIX=/usr

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" all-include BUILDDIR="$BUILD_DIR" TARGET="${HOST##*-}"
}

install () {
	make -C "$SOURCE_DIR" install-include TARGET="${HOST##*-}" BUILDDIR="$BUILD_DIR" DESTDIR="$SYSROOT"
}
