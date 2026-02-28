---
title: tld
---
## infos
git     : [https://github.com/tayoky/tld](https://github.com/tayoky/tld)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tld
./install tld
```

## manifest
```sh
GIT=https://github.com/tayoky/tld
COMMIT=da3e4842ff3f8cdd2c11de7b472ef7bd370398aa

configure() {
	./configure --host="$HOST" --cc="$CC" --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tld](https://github.com/tayoky/ports/blob/main/ports/tld)

