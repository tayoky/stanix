---
title: mesa
---
## infos
version : 25.0.7
website : https://mesa3d.org
tar     : https://mesa3d.org/archive/mesa-25.0.7.tar.xz

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh mesa
./install mesa
```

## manifest
```sh
VERSION="25.0.7"
TAR="https://mesa3d.org/archive/mesa-$VERSION.tar.xz"
WEBSITE="https://mesa3d.org"

configure() {
	mkdir -p build
	cd build

	meson setup .. \
      --prefix=/usr   \
      --buildtype=release \
      --cross-file "$MESON_CROSS" \
      -D platforms=[] \
      -D gallium-drivers=softpipe  \
      -D vulkan-drivers=[]   \
      -D valgrind=disabled \
      -D glx=disabled \
      -D osmesa=true \
      -D llvm=disabled 
}

build() {
	cd build
	meson compile
}

install() {
	meson install --destdir="${PREFIX%%/usr}"
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/mesa

