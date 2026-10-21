---
title: ncurses
---

# Description
version : 6.5  
website : [https://invisible-island.net/ncurses](https://invisible-island.net/ncurses)  
tarball : [https://ftp.gnu.org/ncurses/ncurses-6.5.tar.gz](https://ftp.gnu.org/ncurses/ncurses-6.5.tar.gz)  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install ncurses
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh ncurses
```

# Manifest
```sh
VERSION="6.5"
SOURCE="ncurses"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://invisible-island.net/ncurses"

configure() {
	gnu_configure --with-pkg-config-libdir=/usr/lib/pkgconfig \
	--enable-pc-files \
	--enable-sigwinch \
	--disable-widec \
	--without-ada \
	--without-dlsym \
	--without-cxx-binding
}
```
