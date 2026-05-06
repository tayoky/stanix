---
title: libmpfr
---
## infos
version : 4.2.2  
website : [https://www.mpfr.org/](https://www.mpfr.org/)  
tar     : [https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.2.tar.xz](https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.2.tar.xz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh libmpfr
./install libmpfr
```

## dependencies
This package has dependencies : 
- [libgmp](libgmp)

## manifest
```sh
VERSION="4.2.2"
TAR="$GNU_MIRROR/mpfr/mpfr-$VERSION.tar.xz"
WEBSITE="https://www.mpfr.org/"
DEPENDENCIES="libgmp"

configure() {
	./configure --host="$HOST" --target="$HOST" \
	--prefix="$PREFIX"
}

build() {
	make all -j$NPROC
}

install() {
	make install-strip DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/libmpfr](https://github.com/tayoky/ports/blob/main/ports/libmpfr).

