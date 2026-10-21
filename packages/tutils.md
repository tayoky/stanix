---
title: tutils
---

# Description
version : 0.7.0  
git     : [https://github.com/stanix-org/tutils](https://github.com/stanix-org/tutils)  
commit  : cf9f179b35c711e72a53b389de7637b592062535  

# Dependencies
- [tlibc](tlibc)

# Install
To build and install this package, run  
```sh
./tinx.sh install tutils
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tutils
```

# Manifest
```sh
VERSION="0.7.0"
SOURCE="tutils"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

install () {
	# install in /bin
	make install DESTDIR="$DESTDIR" BINDIR="/bin"
}
```
