---
title: tsed
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tsed
./build.sh tsed
./install.sh tsed
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tsed)  

### manifest
```bash
GIT=https://github.com/StercusMax/tsed
COMMIT=b06cfec6e119e0e1c1771068558a1688c338dde8

configure() {
	true
}

build() {
	$CC main.c -o tsed
}

install() {
	mkdir -p $PREFIX/bin
	cp tsed $PREFIX/bin
}
```

this page was generated using a [script](../../update-packages.md)
