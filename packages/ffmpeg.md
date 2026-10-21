---
title: ffmpeg
---

# Description
version : 8.1.2  
website : [https://www.ffmpeg.org/](https://www.ffmpeg.org/)  
git     : [https://git.ffmpeg.org/ffmpeg](https://git.ffmpeg.org/ffmpeg)  
tag     : n8.1.2  

# Dependencies
- [sdl2-compat](sdl2-compat)
- [zlib](zlib)

# Install
To build and install this package, run  
```sh
./tinx.sh install ffmpeg
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh ffmpeg
```

# Manifest
```sh
VERSION="8.1.2"
SOURCE="ffmpeg"
WEBSITE="https://www.ffmpeg.org/"
DEPENDENCIES="sdl2-compat zlib"

configure () {
	# TODO : remove --disable-asm when we get posix_memalign
	"$SOURCE_DIR/configure" --prefix="$PREFIX" \
	--target-os="none" --arch="${HOST%%-*}" \
	--disable-asm --disable-inline-asm \
	--cc="$HOST-gcc" --cxx="$HOST-g++" \
	--enable-cross-compile \
	--enable-shared \
	--enable-gpl \
	--enable-version3 \
	--disable-openssl
}
```
