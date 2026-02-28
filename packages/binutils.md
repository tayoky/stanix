---
title: binutils
---
## infos
version : 2.44
website : https://www.gnu.org/software/binutils/
tar     : https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh binutils
./install binutils
```

## manifest
```sh
VERSION=2.44
TAR=https://ftp.gnu.org/gnu/binutils/binutils-$VERSION.tar.xz
WEBSITE=https://www.gnu.org/software/binutils/

configure() {
	./configure --host=$HOST \
	--target=$HOST \
	--prefix=/usr \
	--with-sysroot=/ \
	--with-build-sysroot=$SYSROOT \
	--disable-nls --disable-werror \
	--enable-shared \
	CFLAGS="-D_Thread_local=" #stupid tls workaround
}

build(){
	make all -j$NPROC
}

install(){
	make install-strip DESTDIR=${PREFIX%%/usr}
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/binutils

