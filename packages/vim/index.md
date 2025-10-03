---
title: vim
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh vim
./build.sh vim
./install.sh vim
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/vim)  

### manifest
```bash
VERSION="9.1.1485"
#thnanks to bananymous for the github archive trick
TAR="https://github.com/vim/vim/archive/refs/tags/v$VERSION.tar.gz"

configure() {
	./configure --host=$HOST --prefix=/usr \
	--with-pkg-config=$PKG_CONFIG \
	--with-pkg-config-libdir=/usr/lib/pkgconfig \
	--with-tlib=ncurses
	--disable-nls \
	--disable-sysmouse \
	--disable-channel \ 
	vim_cv_toupper_broken=no \
	vim_cv_terminfo=yes \
	vim_cv_tgetent=yes \
	vim_cv_getcwd_broken=no \
	vim_cv_stat_ignores_slash=yes \
	vim_cv_memmove_handles_overlap=yes
}

build(){
	make all -j$NPROC
}

install(){
	make install DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
