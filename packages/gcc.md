---
title: gcc
---

# Description
version : 12.2.0  
website : [https://www.gnu.org/software/gcc/](https://www.gnu.org/software/gcc/)  
tarball : [https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.gz](https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.gz)  

# Dependencies
- [libgmp](libgmp)
- [libmpfr](libmpfr)
- [libmpc](libmpc)
- [binutils](binutils)

# Install
To build and install this package, run  
```sh
./tinx.sh install gcc
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh gcc
```

# Manifest
```sh
VERSION="12.2.0"
SOURCE="gcc"
DEPENDENCIES="libgmp libmpfr libmpc binutils"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/gcc/"

configure () {
	gnu_configure --target="$HOST" \
		--with-sysroot=/ \
		--with-build-sysroot="$SYSROOT" \
		--disable-nls \
		--enable-languages=c,c++ \
		--enable-initfini-array \
		--disable-multilib \
		--enable-shared \
		--with-pic \
		--enable-threads=posix \
		CFLAGS="-D_Thread_local= $CFLAGS" #stupid tls workaround
}
```
