---
title: psf-fonts
---
## infos
version : unknow  
git     : [https://github.com/ercanersoy/PSF-Fonts](https://github.com/ercanersoy/PSF-Fonts)  

## build
To build, once stanix's core is compiled, run
```sh
cd ports
./build.sh psf-fonts
./install psf-fonts
```

## manifest
```sh
GIT=https://github.com/ercanersoy/PSF-Fonts

install() {
	#first setup directory
	mkdir -p "$DESTDIR/$PREFIX/local/share/fonts"

	#copy all psf font
	cp ./*.psf "$DESTDIR/$PREFIX/local/share/fonts/"
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/psf-fonts](https://github.com/tayoky/ports/blob/main/ports/psf-fonts).

