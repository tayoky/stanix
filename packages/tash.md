---
title: tash
---
## infos
git     : https://github.com/tayoky/tash

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
COMMIT=b1dc57dd60829d5c5827f37fe7086b27adcdcc2a

configure() {
	./configure --host="$HOST" --cc=$CC --prefix=$PREFIX
}

build() {
	make
}

install() {
	make install
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/tash

