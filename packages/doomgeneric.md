---
title: doomgeneric
---

# Description
version : unknown  
website : [https://ozkl.github.io/](https://ozkl.github.io/)  
git     : [https://github.com/ozkl/doomgeneric.git](https://github.com/ozkl/doomgeneric.git)  
commit  : dd975839780b4c957c4597f77ccadc3dc592a038  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install doomgeneric
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh doomgeneric
```

# Manifest
```sh
SOURCE="doomgeneric"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	# pretty weird but make must be run in a sub dir
	make -C "$SOURCE_DIR/doomgeneric" -f Makefile.stanix SYSROOT="$SYSROOT" CC="$HOST-gcc"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp "$SOURCE_DIR/doomgeneric/doom" "$DESTDIR$PREFIX/bin"
}
```
