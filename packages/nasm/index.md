---
title: nasm
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh nasm
./build.sh nasm
./install.sh nasm
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/nasm)  

### manifest
```bash
VERSION=2.16.03
TAR=https://www.nasm.us/pub/nasm/releasebuilds/$VERSION/nasm-$VERSION.tar.xz

configure() {
	#thanks bananymous for the --disable-gdb
	./configure --host="$HOST" --prefix="$PREFIX" --disable-gdb
}

build() {
	make
}

install() {
	make install
}
```

this page was generated using a [script](../../update-packages.md)
