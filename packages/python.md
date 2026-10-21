---
title: python
---

# Description
version : 3.14.7  
website : [https://www.python.org/](https://www.python.org/)  
tarball : [https://www.python.org/ftp/python/3.14.7/Python-3.14.7.tar.xz](https://www.python.org/ftp/python/3.14.7/Python-3.14.7.tar.xz)  
hash    : 3b48dac8fb59f62eaa67ac83c1eb12bda1b7a08406dd286e252c11a66be27f81  

# Dependencies
- [stanix-base](stanix-base)
- [libffi](libffi)
- [zlib](zlib)
- [libzstd](libzstd)

# Install
To build and install this package, run  
```sh
./tinx.sh install python
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh python
```

# Manifest
```sh
VERSION="3.14.7"
SOURCE="python"
BUILD_DEPENDENCIES="python"
DEPENDENCIES="stanix-base libffi zlib libzstd"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.python.org/"

configure () {
	gnu_configure --build="$(uname -m)-$(uname -s | tr A-Z a-z)" \
		--with-build-python="python${VERSION%.*}" \
		--with-ensurepip="install" \
		--disable-ipv6 \
		--enable-shared \
		--disable-test-modules \
		ac_cv_file__dev_ptmx=no \
		ac_cv_file__dev_ptc=no
}
```
