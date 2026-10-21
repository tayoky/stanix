---
title: sdl2-compat
---

# Description
version : 2.32.70  
website : [https://libsdl.org/](https://libsdl.org/)  
git     : [https://github.com/libsdl-org/sdl2-compat](https://github.com/libsdl-org/sdl2-compat)  
tag     : release-2.32.70  

# Dependencies
- [sdl3](sdl3)

# Install
To build and install this package, run  
```sh
./tinx.sh install sdl2-compat
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh sdl2-compat
```

# Manifest
```sh
VERSION="2.32.70"
SOURCE="sdl2-compat"
DEPENDENCIES="sdl3"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure \
		-DSDL2COMPAT_X11=OFF -DSDL2COMPAT_TESTS=OFF -DSDL2COMPAT_VENDOR_INFO="Stanix"
}
```
