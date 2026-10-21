---
title: mesa
---

# Description
version : 25.0.7  
website : [https://mesa3d.org](https://mesa3d.org)  
tarball : [https://mesa3d.org/archive/mesa-25.0.7.tar.xz](https://mesa3d.org/archive/mesa-25.0.7.tar.xz)  

# Dependencies
- [zlib](zlib)

# Install
To build and install this package, run  
```sh
./tinx.sh install mesa
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh mesa
```

# Manifest
```sh
VERSION="25.0.7"
SOURCE="mesa"
DEPENDENCIES="zlib"
. "$TOP/scripts/meson-package.sh"
WEBSITE="https://mesa3d.org"

configure () {
	CFLAGS="-Wno-error -Wno-implicit-function-declaration" \
		meson_configure \
		-D platforms=[] \
		-D gallium-drivers=softpipe  \
		-D vulkan-drivers=[]   \
		-D valgrind=disabled \
		-D glx=disabled \
		-D osmesa=true \
		-D llvm=disabled 
}
```
