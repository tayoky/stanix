---
title: ncurses
---
## infos
version : 6.5
website : https://invisible-island.net/ncurses
tar     : https://ftp.gnu.org/gnu/ncurses/ncurses-6.5.tar.gz

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh ncurses
./install ncurses
```

## manifest
```sh
VERSION="6.5"
TAR="https://ftp.gnu.org/gnu/ncurses/ncurses-$VERSION.tar.gz"
WEBSITE=https://invisible-island.net/ncurses

configure() {
	./configure --host=$HOST --prefix=/usr \
	--with-pkg-config=$PKG_CONFIG \
	--with-pkg-config-libdir=/usr/lib/pkgconfig \
	--enable-pc-files \
	--enable-sigwinch \
	--disable-widec \
	--without-ada \
	--without-dlsym \
	--without-cxx-binding
}

build(){
	make all -j$NPROC
}

install(){
	make install DESTDIR=${PREFIX%%/usr}
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/ncurses

