---
title: tcc
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tcc
./build.sh tcc
./install.sh tcc
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tcc)  

### manifest
```bash
GIT=https://github.com/TinyCC/tinycc

configure() {
	./configure --prefix=/ --sysroot=$SYSROOT --targetos=stanix --enable-static --cc=$CC --triplet=x86_64-stanix \
    --sysincludepaths=/usr/include:/usr/lib/tcc/include \
    --libpaths=/usr/lib:/usr/lib/tcc \
    --crtprefix=/usr/lib \
    --elfinterp=/usr/lib/dl.so
    
	echo '#define CONFIG_TCC_STATIC 1
	#define CONFIG_TCC_SEMLOCK 0' >> config.h
}

build() {
	make XTCC=gcc XAR=$AR
}

install() {
	make install DESTDIR=$PREFIX
}
```

this page was generated using a [script](../../update-packages.md)
