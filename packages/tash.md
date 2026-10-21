---
title: tash
---

# Description
version : 0.1.0  
git     : [https://github.com/stanix-org/tash](https://github.com/stanix-org/tash)  
commit  : 5c25113df8b12a55a8f9c0a69e53bb38c8087de6  

# Dependencies
- [tlibc](tlibc)

# Install
To build and install this package, run  
```sh
./tinx.sh install tash
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tash
```

# Manifest
```sh
VERSION="0.1.0"
SOURCE="tash"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install DESTDIR="$DESTDIR" BINDIR="/bin"
}
```
