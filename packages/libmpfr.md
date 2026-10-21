---
title: libmpfr
---

# Description
version : 4.2.2  
website : [https://www.mpfr.org/](https://www.mpfr.org/)  
tarball : [https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.2.tar.xz](https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.2.tar.xz)  

# Dependencies
- [libgmp](libgmp)

# Install
To build and install this package, run  
```sh
./tinx.sh install libmpfr
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libmpfr
```

# Manifest
```sh
VERSION="4.2.2"
SOURCE="libmpfr"
DEPENDENCIES="libgmp"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.mpfr.org/"

configure () {
    gnu_configure --target="$HOST"
}
```
