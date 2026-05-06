---
title: mtools
---
## infos
version : 4.0.49  
website : [https://www.gnu.org/software/mtools/](https://www.gnu.org/software/mtools/)  
tar     : [https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz](https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh mtools
./install mtools
```

## manifest
```sh
VERSION="4.0.49"
TAR="$GNU_MIRROR/mtools/mtools-$VERSION.tar.gz"
WEBSITE="https://www.gnu.org/software/mtools/"

configure() {
	./configure --host="$HOST" --prefix="$PREFIX" CFLAGS=-Wno-error
}

build() {
	make all -j$NPROC
}

install() {
	make install DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/mtools](https://github.com/tayoky/ports/blob/main/ports/mtools).

