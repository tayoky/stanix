---
title: ncurses
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh ncurses
./build.sh ncurses
./install.sh ncurses
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/ncurses)  

### manifest
```bash
VERSION='6.5'
TAR="https://ftp.gnu.org/gnu/ncurses/ncurses-$VERSION.tar.gz"

configure() {
	./configure --host=$HOST --prefix=/usr \
	--with-pkg-config=$PKG_CONFIG \
	--with-pkg-config-libdir=/usr/lib/pkgconfig \
	--enable-pc-files \
	--enable-sigwinch \
	--disable-widec \
	--without-ada \
	--without-dlsym \
	--without-cxx-binding
}

build(){
	make all -j$NPROC
}

install(){
	make install DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
