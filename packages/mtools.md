---
title: mtools
---

# Description
version : 4.0.49  
website : [https://www.gnu.org/software/mtools/](https://www.gnu.org/software/mtools/)  
tarball : [https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz](https://ftp.gnu.org/gnu/mtools/mtools-4.0.49.tar.gz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install mtools
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh mtools
```

# Manifest
```sh
VERSION="4.0.49"
SOURCE="mtools"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/mtools/"

configure () {
	# tlibc's iconv.h is kind of broken
	gnu_configure CFLAGS=-Wno-error ac_cv_header_iconv_h=no
}

install () {
	make install DESTDIR="$DESTDIR"
}
```
