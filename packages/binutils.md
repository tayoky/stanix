---
title: binutils
---

# Description
version : 2.44  
website : [https://www.gnu.org/software/binutils/](https://www.gnu.org/software/binutils/)  
tarball : [https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.gz](https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.gz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install binutils
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh binutils
```

# Manifest
```sh
VERSION="2.44"
SOURCE="binutils"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/binutils/"

configure () {
	gnu_configure --target="$HOST" \
	--with-sysroot=/ \
	--with-build-sysroot="$SYSROOT" \
	--disable-nls --disable-werror \
	--enable-shared \
	CFLAGS="-D_Thread_local= $CFLAGS" # stupid tls workaround
}
```
