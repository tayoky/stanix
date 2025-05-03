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
```ini
git=https://github.com/tayoky/tsh
build="make"
install="make install"
configure=". ${SRC}/configure.sh"
info=info.txt
dependencies=dependencies.txt
patch="tsh.patch"
```

this page was generated using a [script](../../update-packages.md)
