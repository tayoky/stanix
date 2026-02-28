---
title: tcc
---
## infos
website : https://bellard.org/tcc/
git     : https://github.com/TinyCC/tinycc

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
WEBSITE="https://bellard.org/tcc/"

configure() {
	./configure --prefix=/ --sysroot=$SYSROOT --targetos=stanix --enable-static --cc=$CC --triplet=x86_64-stanix \
    --sysincludepaths=/usr/include:/usr/lib/tcc/include \
    --libpaths=/usr/lib:/usr/lib/tcc \
    --crtprefix=/usr/lib \
    --elfinterp=/usr/lib/dl.so
    
	echo '#define CONFIG_TCC_STATIC 1
	#define CONFIG_TCC_SEMLOCK 0' >> config.h
}

build() {
	make XTCC=gcc XAR=$AR
}

install() {
	make install DESTDIR=$PREFIX
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/tcc

