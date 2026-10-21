---
title: libarchive
---

# Description
version : 3.8.8  
website : [https://www.libarchive.org/](https://www.libarchive.org/)  
tarball : [https://www.libarchive.org/downloads/libarchive-3.8.8.tar.xz](https://www.libarchive.org/downloads/libarchive-3.8.8.tar.xz)  

# Dependencies
- [zlib](zlib)
- [libzstd](libzstd)

# Install
To build and install this package, run  
```sh
./tinx.sh install libarchive
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libarchive
```

# Manifest
```sh
VERSION="3.8.8"
SOURCE="libarchive"
DEPENDENCIES="zlib libzstd"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://www.libarchive.org/"
```
