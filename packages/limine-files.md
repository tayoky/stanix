---
title: limine-files
---

# Description
version : unknown  

# Install
To build and install this package, run  
```sh
./tinx.sh install limine-files
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh limine-files
```

# Manifest
```sh
BUILD_DEPENDENCIES="limine-binaries"
PREFIX="/usr"

configure () {
    true
}

build () {
    true
}

install () {
    mkdir -p "$DESTDIR$PREFIX/include/kernel"
    cp "$BUILD_PREFIX/include/limine.h" "$DESTDIR$PREFIX/include/kernel/"
    
	mkdir -p "$DESTDIR/boot/limine"
    for I in limine-bios-cd.bin limine-bios-pxe.bin limine-bios.sys limine-uefi-cd.bin; do
        cp "$BUILD_PREFIX/share/limine/$I" "$DESTDIR/boot/limine"
    done
}
```
