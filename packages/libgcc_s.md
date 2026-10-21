---
title: libgcc_s
---

# Description
version : 12.2.0  

# Install
To build and install this package, run  
```sh
./tinx.sh install libgcc_s
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libgcc_s
```

# Manifest
```sh
VERSION="12.2.0"
BUILD_DEPENDENCIES="gcc"

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
}```
