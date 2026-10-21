---
title: zlib
---

# Description
version : 1.3.2  
website : [https://www.zlib.net/](https://www.zlib.net/)  
tarball : [https://www.zlib.net/zlib-1.3.2.tar.gz](https://www.zlib.net/zlib-1.3.2.tar.gz)  
hash    : bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16  

# Dependencies
- [stanix-base](stanix-base)

# Install
To build and install this package, run  
```sh
./tinx.sh install zlib
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh zlib
```

# Manifest
```sh
VERSION="1.3.2"
SOURCE="zlib"
WEBSITE="https://www.zlib.net/"
DEPENDENCIES="stanix-base"

configure () {
	# thanks to bananymous for the --uname
	CC="$HOST-gcc" AR="$HOST-ar" RANLIB="$HOST-ranlib" \
		"$SOURCE_DIR/configure" --prefix="$PREFIX" --uname=stanix
}
```
