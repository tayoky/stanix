---
title: tlibc
---

# Description
version : 0.0.1  
git     : [https://github.com/tayoky/tlibc](https://github.com/tayoky/tlibc)  
commit  : abd22e399684fa74d53ad65b8fb18def8284ee27  

# Install
To build and install this package, run  
```sh
./tinx.sh install tlibc
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tlibc
```

# Manifest
```sh
VERSION="0.0.1"
SOURCE="tlibc"
BUILD_DEPENDENCIES="binutils gcc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"

configure () {
	tconf_configure --enable-shared --enable-libk
}
```
