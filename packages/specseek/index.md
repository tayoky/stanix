---
title: specseek
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh specseek
./build.sh specseek
./install.sh specseek
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/specseek)  

### manifest
```bash
GIT=https://github.com/Mellurboo/SpecSeek/
COMMIT="fede6a5dad6aeaf664dd65543691ad888cc6d037"

build(){
	make specseek_64 CFLAGS=-mno-sse
}

install(){
	cp bin/gcc/64/specseek_64 $PREFIX/bin/specseek
}
```

this page was generated using a [script](../../update-packages.md)
