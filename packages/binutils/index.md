---
title: binutils
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh binutils
./build.sh binutils
./install.sh binutils
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/binutils)  

### manifest
```bash
TAR=https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz
WEBSITE=https://www.gnu.org/software/binutils/

configure() {
	./configure --host=$HOST \
	--target=$HOST \
	--prefix=/usr \
	--with-sysroot=/ \
	--with-build-sysroot=$SYSROOT \
	--disable-nls --disable-werror \
	CFLAGS="-D_Thread_local=" #stupid tls workaround
}

build(){
	make all -j$NPROC
}

install(){
	make install-strip DESTDIR=${PREFIX%%/usr}
}
```

this page was generated using a [script](../../update-packages.md)
