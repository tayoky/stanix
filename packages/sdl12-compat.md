---
title: sdl12-compat
---

# Description
version : 1.2.76  
website : [https://libsdl.org/](https://libsdl.org/)  
git     : [https://github.com/libsdl-org/sdl12-compat](https://github.com/libsdl-org/sdl12-compat)  
tag     : release-1.2.76  

# Dependencies
- [sdl2-compat](sdl2-compat)

# Install
To build and install this package, run  
```sh
./tinx.sh install sdl12-compat
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh sdl12-compat
```

# Manifest
```sh
VERSION="1.2.76"
SOURCE="sdl12-compat"
DEPENDENCIES="sdl2-compat"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure -DSDL12TESTS=OFF
}

install () {
	cmake_install

	# symlink sdl.pc so old programs can find it
	ln -sf "sdl12_compat.pc" "$DESTDIR$PREFIX/lib/pkgconfig/sdl.pc"
}
```
