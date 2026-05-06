---
title: libgmp
---
## infos
version : 6.3.0  
website : [https://gmplib.org/](https://gmplib.org/)  
tar     : [https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz](https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh libgmp
./install libgmp
```

## manifest
```sh
VERSION=6.3.0
TAR="$GNU_MIRROR/gmp/gmp-$VERSION.tar.xz"
WEBSITE=https://gmplib.org/

configure() {
	./configure --host="$HOST" \
	--prefix="$PREFIX" --disable-cxx
}

build() {
	make all -j$NPROC
}

install() {
	make install-strip DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/libgmp](https://github.com/tayoky/ports/blob/main/ports/libgmp).

