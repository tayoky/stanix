---
title: stanix-kernel-headers
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/kernel`  

# Dependencies
- [tlibc](tlibc)
- [limine-files](limine-files)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-kernel-headers
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-kernel-headers
```

# Manifest
```sh
SOURCE="stanix-kernel"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc limine-files"
. "$TOP/scripts/stanix-package.sh"

configure () {
	true
}

build () {
	true
}

install () {
	make -C "$SOURCE_DIR" install-incs DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR"
}
```
