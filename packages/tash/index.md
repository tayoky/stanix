---
title: tash
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tash
./build.sh tash
./install.sh tash
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tash)  

### manifest
```bash
GIT=https://github.com/tayoky/tash
COMMIT=d78769ee11a8da2f1dd44f820a1f0bb519b8f903

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

this page was generated using a [script](../../update-packages.md)
