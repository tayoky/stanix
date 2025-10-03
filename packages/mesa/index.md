---
title: mesa
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh mesa
./build.sh mesa
./install.sh mesa
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/mesa)  

### manifest
```bash
VERSION=25.0.7
TAR="https://mesa3d.org/archive/mesa-$VERSION.tar.xz"

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

this page was generated using a [script](../../update-packages.md)
