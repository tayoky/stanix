---
title: doomgeneric
---
## infos
website : [https://ozkl.github.io/](https://ozkl.github.io/)  
git     : [https://github.com/ozkl/doomgeneric.git](https://github.com/ozkl/doomgeneric.git)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh doomgeneric
./install doomgeneric
```

## manifest
```sh
GIT=https://github.com/ozkl/doomgeneric.git
COMMIT=dd975839780b4c957c4597f77ccadc3dc592a038
WEBSITE=https://ozkl.github.io/

build() {
	#pretty weird but make must be run in a sub dir
	cd doomgeneric
	make -f Makefile.stanix SYSROOT="$SYSROOT"
}

install() {
	cd doomgeneric
	cp ./doom "${PREFIX}/bin"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/doomgeneric](https://github.com/tayoky/ports/blob/main/ports/doomgeneric)

