---
title: mtools
---
## infos
version : 4.0.49
website : https://www.gnu.org/software/mtools/
tar     : http://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh mtools
./install mtools
```

## manifest
```sh
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
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/mtools

