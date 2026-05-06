---
title: libmpc
---
## infos
version : 1.4.1  
website : [https://www.multiprecision.org/](https://www.multiprecision.org/)  
tar     : [https://ftp.gnu.org/gnu/mpc/mpc-1.4.1.tar.xz](https://ftp.gnu.org/gnu/mpc/mpc-1.4.1.tar.xz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh libmpc
./install libmpc
```

## dependencies
This package has dependencies : 
- [libgmp](libgmp)
- [libmpfr](libmpfr)

## manifest
```sh
VERSION="1.4.1"
TAR="$GNU_MIRROR/mpc/mpc-$VERSION.tar.xz"
WEBSITE="https://www.multiprecision.org/"
DEPENDENCIES="libgmp libmpfr"

configure() {
	./configure --host="$HOST" --target="$HOST" \
	--prefix="$PREFIX" --with-sysroot="$SYSROOT"
}

build(){
	make all -j$NPROC
}

install(){
	make install-strip DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/libmpc](https://github.com/tayoky/ports/blob/main/ports/libmpc).

