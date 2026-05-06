---
title: tcc
---
## infos
version : unknow  
website : [https://bellard.org/tcc/](https://bellard.org/tcc/)  
git     : [https://github.com/TinyCC/tinycc](https://github.com/TinyCC/tinycc)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tcc
./install tcc
```

## manifest
```sh
GIT="https://github.com/TinyCC/tinycc"
COMMIT="fada98b1ce9ee0c36771183177218828ccb9b9de"
WEBSITE="https://bellard.org/tcc/"

configure() {
	./configure --prefix=$PREFIX --sysroot=$SYSROOT --targetos=stanix --enable-static --cc=$CC --triplet=x86_64-stanix \
    --sysincludepaths=/usr/include:$PREFIX/lib/tcc/include \
    --libpaths=/usr/lib:$PREFIX/lib/tcc \
    --crtprefix=/usr/lib \
    --elfinterp=/usr/lib/ld-tlibc.so
    
	echo '#define CONFIG_TCC_SEMLOCK 0' >> config.h
}

build() {
	make XTCC=gcc XAR=$AR
}

install() {
	make install DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tcc](https://github.com/tayoky/ports/blob/main/ports/tcc).

