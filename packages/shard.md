---
title: shard
---
## infos
git     : [https://github.com/shardlanguage/shard/](https://github.com/shardlanguage/shard/)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh shard
./install shard
```

## manifest
```sh
GIT=https://github.com/shardlanguage/shard/
COMMIT=3f72898d42f42b6a7d4bc2767f724748b77239e0

build(){
    make -C scripts CC=$CC
}

install(){
    cp shard $PREFIX/bin
}```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/shard](https://github.com/tayoky/ports/blob/main/ports/shard)

