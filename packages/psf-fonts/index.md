---
title: psf-fonts
comment: this file was generated automaticly DO NOT EDIT
---
## description

## build
to build and install this package use the ports submodule in the stanix repo
after having making stanix
```sh
cd ports
./clean.sh psf-fonts
./build.sh psf-fonts
./install.sh psf-fonts
```

## precompiled
precompiled are currently not available

## packages source
[package source](https://github.com/tayoky/ports/tree/main/ports/psf-fonts)  

### manifest
```bash
GIT=https://github.com/ercanersoy/PSF-Fonts

install() {
	#first setup directory
	mkdir -p ${PREFIX}/local/share/consolefonts

	#copy all psf font
	cp ./*.psf ${PREFIX}/local/share/consolefonts/
}
```

this page was generated using a [script](../../update-packages.md)
