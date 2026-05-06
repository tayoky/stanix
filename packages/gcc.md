---
title: gcc
---
## infos
version : 12.2.0  
website : [https://www.gnu.org/software/gcc/](https://www.gnu.org/software/gcc/)  
tar     : [https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.xz](https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.xz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh gcc
./install gcc
```

## dependencies
This package has dependencies : 
- [libgmp](libgmp)
- [libmpfr](libmpfr)
- [libmpc](libmpc)

## manifest
```sh
VERSION="12.2.0"
TAR="$GNU_MIRROR/gcc/gcc-$VERSION/gcc-$VERSION.tar.xz"
WEBSITE="https://www.gnu.org/software/gcc/"
DEPENDENCIES="libgmp libmpfr libmpc"

configure() {
	./configure --host="$HOST" \
	--target="$HOST" \
	--prefix="$PREFIX" \
	--with-sysroot=/ \
	--with-build-sysroot=$SYSROOT \
	--disable-nls --disable-werror \
	--enable-shared \
	--disable-multilib --enable-shared --enable-threads=posix \
	--enable-languages=c,c++ \
	CFLAGS="-D_Thread_local=" #stupid tls workaround
}

build() {
	make all-gcc -j$NPROC
	make all-target-libgcc -j$NPROC
	make all-target-libstdc++-v3 -j$NPROC
}

install() {
	make install-strip-gcc DESTDIR="$DESTDIR"
	make install-strip-target-libgcc DESTDIR="$DESTDIR"
	make install-strip-target-libstdc++-v3 DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/gcc](https://github.com/tayoky/ports/blob/main/ports/gcc).

