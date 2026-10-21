---
title: sdl3
---

# Description
version : 3.4.10  
website : [https://libsdl.org/](https://libsdl.org/)  
git     : [https://github.com/libsdl-org/SDL](https://github.com/libsdl-org/SDL)  
tag     : release-3.4.10  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install sdl3
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh sdl3
```

# Manifest
```sh
VERSION="3.4.10"
SOURCE="sdl3"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure \
		-DSDL_GPU=OFF \
		-DSDL_CAMERA=OFF \
		-DSDL_HAPTIC=OFF \
		-DSDL_HIDAPI=OFF \
		-DSDL_POWER=OFF \
		-DSDL_SENSOR=OFF \
		-DSDL_DIALOG=OFF \
		-DSDL_TESTS=OFF \
		-DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_DISKAUDIO=OFF \
		-DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_VULKAN=OFF -DSDL_DIRECTX=OFF -DSDL_KMSDRM=OFF -DSDL_DBUS=OFF -DSDL_UNIX_CONSOLE_BUILD=ON \
		-DSDL_SHARED=ON -DSDL_VENDOR_INFO="Stanix"
}
```
