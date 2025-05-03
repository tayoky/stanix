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
```ini
patch=doomgeneric.patch
git=https://github.com/ozkl/doomgeneric.git
commit=dd975839780b4c957c4597f77ccadc3dc592a038
website=https://ozkl.github.io/
build=". ${SRC}/build.sh"
install=". ${SRC}/install.sh"
dependencies=dependencies.txt
```

this page was generated using a [script](../../update-packages.md)
