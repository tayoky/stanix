---
title: tsed
---
## infos
git     : [https://github.com/StercusMax/tsed](https://github.com/StercusMax/tsed)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tsed
./install tsed
```

## manifest
```sh
GIT=https://github.com/StercusMax/tsed
COMMIT=b06cfec6e119e0e1c1771068558a1688c338dde8

configure() {
	true
}

build() {
	$CC main.c -o tsed
}

install() {
	mkdir -p $PREFIX/bin
	cp tsed $PREFIX/bin
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tsed](https://github.com/tayoky/ports/blob/main/ports/tsed)

