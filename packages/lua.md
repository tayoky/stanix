---
title: lua
---

# Description
version : 5.5.1  
tarball : [https://www.lua.org/ftp/lua-5.5.1.tar.gz](https://www.lua.org/ftp/lua-5.5.1.tar.gz)  
hash    : 1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install lua
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh lua
```

# Manifest
```sh
VERSION="5.5.1"
SOURCE="lua"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" -j$NPROC \
		CC="$HOST-gcc" AR="$HOST-ar rcu" MYCFLAGS="-g"
}

install () {
	make -C "$SOURCE_DIR" install INSTALL_TOP="$DESTDIR$PREFIX" \
		CC="$HOST-gcc" AR="$HOST-ar rcu" MYCFLAGS="-g"
}
```
