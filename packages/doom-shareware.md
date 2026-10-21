---
title: doom-shareware
---

# Description
version : 1.8  

# Dependencies
- [doomgeneric](doomgeneric)

# Install
To build and install this package, run  
```sh
./tinx.sh install doom-shareware
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh doom-shareware
```

# Manifest
```sh
VERSION="1.8"
DEPENDENCIES="doomgeneric"

configure () {
	tinx_download "https://www.gamers.org/pub/idgames/idstuff/doom/doom-1.8.wad.gz" "doom-$VERSION.wad.gz"
}

build () {
	gunzip -kf "doom-$VERSION.wad.gz" || return 1
	chmod 0644 "doom-$VERSION.wad"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/games/doom"
	cp -p "doom-$VERSION.wad" "$DESTDIR$PREFIX/games/doom/doom1.wad"
}
```
