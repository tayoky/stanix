---
title: tash
---
## infos
version : unknow  
git     : [https://github.com/tayoky/tash](https://github.com/tayoky/tash)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tash
./install tash
```

## manifest
```sh
GIT=https://github.com/tayoky/tash
COMMIT=5bc2f93e9b1a4a0f016f4e15404153eb2b3d3aab

configure() {
	./configure --host="$HOST" --cc=$CC --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tash](https://github.com/tayoky/ports/blob/main/ports/tash).

