---
title: libmpc
---

# Description
version : 1.4.1  
website : [https://www.multiprecision.org/](https://www.multiprecision.org/)  
tarball : [https://ftp.gnu.org/gnu/mpc/mpc-1.4.1.tar.xz](https://ftp.gnu.org/gnu/mpc/mpc-1.4.1.tar.xz)  

# Dependencies
- [libgmp](libgmp)
- [libmpfr](libmpfr)

# Install
To build and install this package, run  
```sh
./tinx.sh install libmpc
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh libmpc
```

# Manifest
```sh
VERSION="1.4.1"
SOURCE="libmpc"
DEPENDENCIES="libgmp libmpfr"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.multiprecision.org/"

configure () {
    gnu_configure --target="$HOST" --with-sysroot="$SYSROOT"
}```
