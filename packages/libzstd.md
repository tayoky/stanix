---
title: libzstd
---

# Description
version : 0.4.2  
git     : [https://github.com/facebook/zstd](https://github.com/facebook/zstd)  
tag     : zstd-0.4.2  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install libzstd
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libzstd
```

# Manifest
```sh
VERSION="0.4.2"
SOURCE="libzstd"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" default -j"$PARALLELISM"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR"
}
```
