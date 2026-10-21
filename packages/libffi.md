---
title: libffi
---

# Description
version : 3.8.0  
tarball : [https://github.com/libffi/libffi/releases/download/v3.8.0/libffi-3.8.0.tar.gz](https://github.com/libffi/libffi/releases/download/v3.8.0/libffi-3.8.0.tar.gz)  
hash    : 7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install libffi
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libffi
```

# Manifest
```sh
VERSION="3.8.0"
SOURCE="libffi"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
```
