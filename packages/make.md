---
title: make
---

# Description
version : 4.4.1  
website : [https://www.gnu.org/software/make](https://www.gnu.org/software/make)  
tarball : [https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz](https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install make
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh make
```

# Manifest
```sh
VERSION="4.4.1"
SOURCE="make"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/make"

configure () {
	gnu_configure --without-guile --disable-job-server --disable-thread --disable-nls --disable-posix-spawn --enable-year-2038
}
```
