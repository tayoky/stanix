---
title: stanix-kernel
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/kernel`  

# Dependencies
- [tlibc](tlibc)
- [stanix-kernel-headers](stanix-kernel-headers)
- [stanix-libraries](stanix-libraries)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-kernel
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-kernel
```

# Manifest
```sh
SOURCE="stanix-kernel"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc stanix-kernel-headers stanix-libraries"
. "$TOP/scripts/stanix-package.sh"

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" BUILDDIR="$BUILD_DIR"
}

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" NM="$HOST-nm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" BINDIR="/boot"
}
```
