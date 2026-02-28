---
title: zlib
---
## infos
version : 1.3.1  
website : [https://www.zlib.net/](https://www.zlib.net/)  
tar     : [https://www.zlib.net/zlib-1.3.1.tar.gz](https://www.zlib.net/zlib-1.3.1.tar.gz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh zlib
./install zlib
```

## manifest
```sh
VERSION="1.3.1"
TAR="https://www.zlib.net/zlib-$VERSION.tar.gz"
WEBSITE="https://www.zlib.net/"

configure() {
	#thanks to bananymous for the --uname
	./configure --prefix=/usr --uname=stanix
}

build() {
	make -j$NPROC
}

install() {
	make install DESTDIR=${PREFIX%%/usr}
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/zlib](https://github.com/tayoky/ports/blob/main/ports/zlib)

