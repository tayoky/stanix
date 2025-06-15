---
title: tsh
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tsh
./build.sh tsh
./install.sh tsh
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tsh)  

### manifest
```bash
GIT=https://github.com/tayoky/tsh
COMMIT=529c3a4bca9677b8194fa51916d98ba9e16e3eb0

configure() {
	./configure --host="$HOST" --with-CC="$CC" --prefix="$PREFIX"

	#we need to specify custom CFLAGS
	echo 'CFLAGS = -fno-stack-protector \
		-fno-stack-check \
		-fno-PIC \
		-mno-80387 \
		-mno-mmx \
		-mno-red-zone '>> config.mk
}

build() {
	make
}

install() {
	make install
}
```

this page was generated using a [script](../../update-packages.md)
