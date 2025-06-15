---
title: doomgeneric
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh doomgeneric
./build.sh doomgeneric
./install.sh doomgeneric
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/doomgeneric)  

### manifest
```bash
GIT=https://github.com/ozkl/doomgeneric.git
COMMIT=dd975839780b4c957c4597f77ccadc3dc592a038
WEBSITE=https://ozkl.github.io/

build() {
	#pretty weird but make must be run in a sub dir
	cd doomgeneric
	make -f Makefile.stanix SYSROOT="$SYSROOT"
}

install() {
	cd doomgeneric
	cp ./doom "${PREFIX}/bin"
}
```

this page was generated using a [script](../../update-packages.md)
