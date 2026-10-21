---
title: stanix-modules
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/modules`  

# Dependencies
- [stanix-kernel](stanix-kernel)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-modules
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-modules
```

# Manifest
```sh
SOURCE="stanix-modules"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="stanix-kernel"
. "$TOP/scripts/stanix-package.sh"

install () {
	make -C "$SOURCE_DIR" install CC="$HOST-gcc" NASM="nasm" DESTDIR="$DESTDIR" BUILDDIR="$BUILD_DIR" MODDIR="/mod"
}
```
