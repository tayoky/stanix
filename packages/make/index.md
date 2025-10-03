---
title: make
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh make
./build.sh make
./install.sh make
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/make)  

### manifest
```bash
VERSION='4.4.1'
TAR="https://ftp.gnu.org/gnu/make/make-$VERSION.tar.gz"

configure() {
	./configure --host="$HOST" --prefix=/usr  --without-guile --disable-job-server --disable-thread --disable-nls --disable-posix-spawn --enable-year-2038
}

build() {
	make -j$NPROC
}

install() {
	make install DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
