---
title: stanix-libraries
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/libraries`  

# Dependencies
- [tlibc](tlibc)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-libraries
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-libraries
```

# Manifest
```sh
SOURCE="stanix-libraries"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/stanix-package.sh"

configure () {
	(cd "$SOURCE_DIR/libtgui" && ./configure --prefix="$PREFIX" --host="$HOST" --builddir="$BUILD_DIR/libtgui")
}
```
