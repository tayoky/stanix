---
title: sdl2-mixer
---

# Description
version : 2.8.2  
website : [https://libsdl.org/](https://libsdl.org/)  
git     : [https://github.com/libsdl-org/SDL_mixer](https://github.com/libsdl-org/SDL_mixer)  
tag     : release-2.8.2  

# Dependencies
- [sdl2-compat](sdl2-compat)

# Install
To build and install this package, run  
```sh
./tinx.sh install sdl2-mixer
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh sdl2-mixer
```

# Manifest
```sh
VERSION="2.8.2"
SOURCE="sdl2-mixer"
DEPENDENCIES="sdl2-compat"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure \
		-DSDL2MIXER_DEPS_SHARED=OFF \
		-DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
		-DSDL2MIXER_WAVPACK=OFF \
		-DSDL2MIXER_OPUS=OFF \
		-DSDL2MIXER_MOD=OFF
}
```
