---
title: shard
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh shard
./build.sh shard
./install.sh shard
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/shard)  

### manifest
```bash
GIT=https://github.com/shardlanguage/shard/
COMMIT=3f72898d42f42b6a7d4bc2767f724748b77239e0

build(){
    make -C scripts CC=$CC
}

install(){
    cp shard $PREFIX/bin
}```

this page was generated using a [script](../../update-packages.md)
