---
title: nyancat
---

# Description
version : 1.5.2  
git     : [https://github.com/klange/nyancat](https://github.com/klange/nyancat)  
tag     : 1.5.2  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install nyancat
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh nyancat
```

# Manifest
```sh
VERSION="1.5.2"
SOURCE="nyancat"
DEPENDENCIES="stanix-base"

configure () {
    true
}

build () {
    make -C "$SOURCE_DIR" CC="$HOST-gcc"
}

install () {
    mkdir -p "$DESTDIR$PREFIX/bin"
    mkdir -p "$DESTDIR$PREFIX/share/man1"
	cp -p "$SOURCE_DIR/src/nyancat" "$DESTDIR$PREFIX/bin/"
	cp -p "$SOURCE_DIR/nyancat.1" "$DESTDIR$PREFIX/share/man1/"
}
```
