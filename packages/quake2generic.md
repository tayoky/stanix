---
title: quake2generic
---
## infos
version : unknow  
git     : [https://github.com/ozkl/quake2generic.git](https://github.com/ozkl/quake2generic.git)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh quake2generic
./install quake2generic
```

## manifest
```sh
GIT=https://github.com/ozkl/quake2generic.git
COMMIT=74d48cdb25a393032c6706344d406dc4f6f16ff3

build() {
	make -f Makefile-stanix
}

install() {
	mkdir -p "$DESTDIR/$PREFIX/bin"
	cp build/quake2 "$DESTDIR/$PREFIX/bin"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/quake2generic](https://github.com/tayoky/ports/blob/main/ports/quake2generic).

