---
title: nyancat
---
## infos
version : unknow  
git     : [https://github.com/klange/nyancat](https://github.com/klange/nyancat)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh nyancat
./install nyancat
```

## manifest
```sh
GIT="https://github.com/klange/nyancat"
COMMIT="32fd2eb40332ae0001995705f0c1f8de69a2d543"

configure() {
	true
}

build() {
	make
}

install() {
	mkdir -p "$DESTDIR/$PREFIX/bin"
	cp src/nyancat "$DESTDIR/$PREFIX/bin"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/nyancat](https://github.com/tayoky/ports/blob/main/ports/nyancat).

