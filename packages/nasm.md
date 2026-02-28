---
title: nasm
---
## infos
version : 2.16.03  
website : [https://nasm.us](https://nasm.us)  
tar     : [https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz](https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh nasm
./install nasm
```

## manifest
```sh
VERSION="2.16.03"
TAR="https://www.nasm.us/pub/nasm/releasebuilds/$VERSION/nasm-$VERSION.tar.xz"
WEBSITE="https://nasm.us"

configure() {
	#thanks bananymous for the --disable-gdb
	./configure --host="$HOST" --prefix="$PREFIX" --disable-gdb
}

build() {
	make
}

install() {
	make install
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/nasm](https://github.com/tayoky/ports/blob/main/ports/nasm)

