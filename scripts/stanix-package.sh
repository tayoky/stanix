# helper script to build stanix package
WEBSITE="https://tayoky.github.io/stanix"
VERSION="0.2.1-indev"

# first party packages go into /usr
PREFIX="/usr"

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
