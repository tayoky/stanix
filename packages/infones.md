---
title: infones
---

# Description
version : unknown  
git     : [https://github.com/jay-kumogata/InfoNES](https://github.com/jay-kumogata/InfoNES)  
commit  : 4debf50a819ea3de9ddbd267ea4bc95560f35146  

# Dependencies
- [sdl12-compat](sdl12-compat)

# Install
To build and install this package, run  
```sh
./tinx.sh install infones
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh infones
```

# Manifest
```sh
SOURCE="infones"
DEPENDENCIES="sdl12-compat"
. "$TOP/scripts/cmake-package.sh"

configure () {
	cmake_configure \
		-DSDL_INCLUDE_DIR="$SYSROOT$PREFIX/include/SDL" \
		-DSDL_LIBRARY="-lSDL"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp InfoNES "$DESTDIR$PREFIX/bin"
}
```
