---
title: specseek
---
## infos
git     : [https://github.com/Mellurboo/SpecSeek/](https://github.com/Mellurboo/SpecSeek/)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh specseek
./install specseek
```

## manifest
```sh
GIT=https://github.com/Mellurboo/SpecSeek/
COMMIT="fede6a5dad6aeaf664dd65543691ad888cc6d037"

build(){
	make specseek_64 CFLAGS=-mno-sse
}

install(){
	cp bin/gcc/64/specseek_64 $PREFIX/bin/specseek
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/specseek](https://github.com/tayoky/ports/blob/main/ports/specseek)

