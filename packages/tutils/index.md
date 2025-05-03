---
title: tutils
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh tutils
./build.sh tutils
./install.sh tutils
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/tutils)  

### manifest
```ini
git=https://github.com/tayoky/tutils
build="make"
install="make install"
configure=". ${SRC}/configure.sh"
patch="tutils.patch"```

this page was generated using a [script](../../update-packages.md)
