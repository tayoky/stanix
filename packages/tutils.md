---
title: tutils
---
## infos
git     : [https://github.com/tayoky/tutils](https://github.com/tayoky/tutils)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh tutils
./install tutils
```

## manifest
```sh
GIT=https://github.com/tayoky/tutils
COMMIT=a58515420b950ca10d9e3957b8c07216ca188b0c

configure() {
	./configure --host="$HOST" --with-CC="$CC" --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tutils](https://github.com/tayoky/ports/blob/main/ports/tutils)

