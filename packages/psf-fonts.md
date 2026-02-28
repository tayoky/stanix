---
title: psf-fonts
---
## infos
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
	mkdir -p ${PREFIX}/local/share/consolefonts

	#copy all psf font
	cp ./*.psf ${PREFIX}/local/share/consolefonts/
}
```
This package manifest and it's associed patches can be found at [https://github.com/tayoky/ports/blob/main/ports/psf-fonts](https://github.com/tayoky/ports/blob/main/ports/psf-fonts)

