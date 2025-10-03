---
title: nyancat
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh nyancat
./build.sh nyancat
./install.sh nyancat
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/nyancat)  

### manifest
```bash
GIT="https://github.com/klange/nyancat"
COMMIT="32fd2eb40332ae0001995705f0c1f8de69a2d543"

configure() {
	true
}

build() {
	make
}

install() {
	cp src/nyancat $PREFIX/bin
}
```

this page was generated using a [script](../../update-packages.md)
