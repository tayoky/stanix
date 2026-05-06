---
title: tutils
---
## infos
version : unknow  
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
COMMIT=ba00c74218b91097fd592840e05ae2edb763407c

configure() {
	./configure --host="$HOST" --with-CC="$CC" --prefix="$PREFIX"
}

build() {
	make
}

install() {
	make install DESTDIR="$DESTDIR"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/tutils](https://github.com/tayoky/ports/blob/main/ports/tutils).

