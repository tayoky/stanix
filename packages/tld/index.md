---
title: tld
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tld
./build.sh tld
./install.sh tld
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tld)  

### manifest
```bash
GIT=https://github.com/tayoky/tld
COMMIT=da3e4842ff3f8cdd2c11de7b472ef7bd370398aa

configure() {
	./configure --host="$HOST" --cc="$CC" --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install
}
```

this page was generated using a [script](../../update-packages.md)
