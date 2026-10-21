---
title: nasm
---

# Description
version : 2.16.03  
website : [https://nasm.us](https://nasm.us)  
tarball : [https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz](https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install nasm
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh nasm
```

# Manifest
```sh
VERSION="2.16.03"
SOURCE="nasm"
WEBSITE="https://nasm.us"
DEPENDENCIES="stanix-base"

configure () {
	# thanks bananymous for the --disable-gdb
	"$SOURCE_DIR/configure" --host="$HOST" --prefix="$PREFIX" --disable-gdb
}
```
