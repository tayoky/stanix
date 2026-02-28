---
title: kritic
---
## infos
git     : https://github.com/Wrench56/KritiC

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh kritic
./install kritic
```

## manifest
```sh
GIT=https://github.com/Wrench56/KritiC

build() {
	make static CC=$CC LD=$LD CFLAGS=-DKRITIC_DISABLE_REDIRECT PLATFORM=stanix
}

install() {
	mkdir -p $PREFIX/lib && cp build/libkritic.a $PREFIX/lib
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/kritic

