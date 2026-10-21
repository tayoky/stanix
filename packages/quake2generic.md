---
title: quake2generic
---

# Description
version : unknown  
git     : [https://github.com/ozkl/quake2generic](https://github.com/ozkl/quake2generic)  
commit  : 74d48cdb25a393032c6706344d406dc4f6f16ff3  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install quake2generic
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh quake2generic
```

# Manifest
```sh
SOURCE="quake2generic"
DEPENDENCIES="stanix-base"

build () {
	make -C "$SOURCE_DIR" CC="$HOST-gcc" -f Makefile-stanix -j"$PARALLELISM" BUILDDIR="$BUILD_DIR"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/bin"
	cp "$BUILD_DIR/quake2" "$DESTDIR$PREFIX/bin"
	"$HOST-strip" "$DESTDIR$PREFIX/bin/quake2"
}
```
