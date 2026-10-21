---
title: tvi
---

# Description
version : 0.0.1  
git     : [https://github.com/stanix-org/tvi](https://github.com/stanix-org/tvi)  
commit  : 8e8fa25c85ed7b27b55a055a50cd9b4aa43a466a  

# Dependencies
- [tlibc](tlibc)

# Install
To build and install this package, run  
```sh
./tinx.sh install tvi
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tvi
```

# Manifest
```sh
VERSION="0.0.1"
SOURCE="tvi"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="tlibc"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"
```
