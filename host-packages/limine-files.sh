BUILD_DEPENDENCIES="limine-binaries"

configure () {
    true
}

build () {
    true
}

install () {
    mkdir -p "$DESTDIR/boot/limine"
    cp "$TOP/limine.conf" "$DESTDIR/boot/limine"
    for I in limine-bios-cd.bin limine-bios-pxe.bin limine-bios.sys limine-uefi-cd.bin; do
        cp "$BUILD_PREFIX/share/limine/$I" "$DESTDIR/boot/limine"
    done
}
