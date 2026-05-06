---
title: tvi
---
## infos
version : unknow  
git     : [https://github.com/tayoky/tvi](https://github.com/tayoky/tvi)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tvi
./install tvi
```

## manifest
```sh
GIT=https://github.com/tayoky/tvi
COMMIT=20929ba6923388f689ef3e554dcc27aefd767f03

configure() {
	# disable dynamic linking linking
	./configure --host="$HOST" --cc="$CC" --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tvi](https://github.com/tayoky/ports/blob/main/ports/tvi).

