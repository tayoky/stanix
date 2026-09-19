# helper script to build stanix package
WEBSITE="https://tayoky.github.io/stanix"
VERSION="0.2.1-indev"

# first party packages go into /usr
PREFIX="/usr"

configure () {
	true
}

build () {
	CC="$HOST-gcc" \
	AR="$HOST-ar" \
	make -C "$SOURCE_DIR"  BUILDDIR="$BUILD_DIR"
}

install () {
	CC="$HOST-gcc" \
	AR="$HOST-ar" \
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
