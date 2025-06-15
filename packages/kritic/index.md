---
title: kritic
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh kritic
./build.sh kritic
./install.sh kritic
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/kritic)  

### manifest
```ini
GIT=https://github.com/Wrench56/KritiC

build() {
	make static CC=$CC LD=$LD CFLAGS=-DKRITIC_DISABLE_REDIRECT PLATFORM=stanix
}

install() {
	mkdir -p $PREFIX/lib && cp build/libkritic.a $PREFIX/lib
}
```

this page was generated using a [script](../../update-packages.md)
