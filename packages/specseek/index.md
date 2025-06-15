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
```ini
GIT=https://github.com/Mellurboo/SpecSeek/
COMMIT="47f51c62de8e8142a8ecb47b65b19cc7fd60b2c7"

build(){
	make specseek_64 CFLAGS=-mno-sse
}

install(){
	cp bin/gcc/64/specseek_64 $PREFIX/bin/specseek
}
```

this page was generated using a [script](../../update-packages.md)
