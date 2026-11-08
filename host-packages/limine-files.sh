BUILD_DEPENDENCIES="tool-limine-binaries"
PREFIX="/usr"

configure () {
    true
}

build () {
    true
}

install () {
    LIMINE_PREFIX="$BUILD_PREFIX/tool-limine-binaries"
    
    mkdir -p "$DESTDIR$PREFIX/include/kernel"
    cp "$LIMINE_PREFIX/include/limine.h" "$DESTDIR$PREFIX/include/kernel/"
    
	mkdir -p "$DESTDIR/boot/limine"
    for I in limine-bios-cd.bin limine-bios-pxe.bin limine-bios.sys limine-uefi-cd.bin; do
        cp "$LIMINE_PREFIX/share/limine/$I" "$DESTDIR/boot/limine"
    done
}
