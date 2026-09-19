VERSION="9.3.3"
SOURCE="limine-binaries"
WEBSITE="https://github.com/Limine-Bootloader/Limine"

configure () {
    true
}

build () {
    make -C "$SOURCE_DIR"
}

install () {
    make -C "$SOURCE_DIR" install-strip DESTDIR="$DESTDIR" PREFIX="$PREFIX"
}
