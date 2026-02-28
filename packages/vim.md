---
title: vim
---
## infos
version : 9.1.1485
website : https://www.vim.org
tar     : https://github.com/vim/vim/archive/refs/tags/v9.1.1485.tar.gz

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh vim
./install vim
```

## manifest
```sh
VERSION="9.1.1485"
#thanks to bananymous for the github archive trick
TAR="https://github.com/vim/vim/archive/refs/tags/v$VERSION.tar.gz"
WEBSITE="https://www.vim.org"

configure() {
	./configure --host=$HOST --prefix=/usr \
	--with-pkg-config=$PKG_CONFIG \
	--with-pkg-config-libdir=/usr/lib/pkgconfig \
	--with-tlib=ncurses
	--disable-nls \
	--disable-sysmouse \
	--disable-channel \ 
	vim_cv_toupper_broken=no \
	vim_cv_terminfo=yes \
	vim_cv_tgetent=yes \
	vim_cv_getcwd_broken=no \
	vim_cv_stat_ignores_slash=yes \
	vim_cv_memmove_handles_overlap=yes
}

build(){
	make all -j$NPROC
}

install(){
	make install DESTDIR=${PREFIX%%/usr}
}
```
This package manifest and it's associed patches can be found at https://github.com/tayoky/ports/blob/main/ports/vim

