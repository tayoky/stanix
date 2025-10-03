---
title: mtools
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh mtools
./build.sh mtools
./install.sh mtools
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/mtools)  

### manifest
```bash
VERSION=4.0.49
TAR=http://ftp.gnu.org/gnu/mtools/mtools-$VERSION.tar.gz
WEBSITE=https://www.gnu.org/software/mtools/

configure() {
	./configure --host=$HOST --prefix=/usr CFLAGS=-Wno-error
}

build(){
	make all -j$NPROC
}

install(){
	make install DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
