---
title: zlib
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh zlib
./build.sh zlib
./install.sh zlib
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/zlib)  

### manifest
```bash
VERSION=1.3.1
TAR=https://www.zlib.net/zlib-$VERSION.tar.gz

configure() {
	#thanks to bananymous for the --uname
	./configure --prefix=/usr --uname=stanix
}

build() {
	make -j$NPROC
}

install() {
	make install DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
