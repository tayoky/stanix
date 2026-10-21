---
title: libgmp
---

# Description
version : 6.3.0  
website : [https://gmplib.org/](https://gmplib.org/)  
tarball : [https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz](https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install libgmp
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libgmp
```

# Manifest
```sh
VERSION="6.3.0"
SOURCE="libgmp"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://gmplib.org/"

configure () {
    gnu_configure --disable-cxx
}
```
