---
title: make
---
## infos
version : 4.4.1  
website : [https://www.gnu.org/software/make](https://www.gnu.org/software/make)  
tar     : [https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz](https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh make
./install make
```

## manifest
```sh
VERSION="4.4.1"
TAR="https://ftp.gnu.org/gnu/make/make-$VERSION.tar.gz"
WEBSITE="https://www.gnu.org/software/make"

configure() {
	./configure --host="$HOST" --prefix=/usr  --without-guile --disable-job-server --disable-thread --disable-nls --disable-posix-spawn --enable-year-2038
}

build() {
	make -j$NPROC
}

install() {
	make install DESTDIR=${PREFIX%%/usr}
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/make](https://github.com/tayoky/ports/blob/main/ports/make)

