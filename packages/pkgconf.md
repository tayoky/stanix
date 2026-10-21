---
title: pkgconf
---

# Description
version : 2.3.0  
tarball : [https://distfiles.ariadne.space/pkgconf/pkgconf-2.3.0.tar.gz](https://distfiles.ariadne.space/pkgconf/pkgconf-2.3.0.tar.gz)  
hash    : a2df680578e85f609f2fa67bd3d0fc0dc71b4bf084fc49119de84cd6ed28e723  

# Install
To build and install this package, run  
```sh
./tinx.sh install pkgconf
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh pkgconf
```

# Manifest
```sh
VERSION="2.3.0"
SOURCE="pkgconf"
BUILD_DEPENDENCIES="autoconf automake libtool"
. "$TOP/scripts/gnu-package.sh"

configure () {
	gnu_configure --enable-shared
}

install () {
	make install DESTDIR="$DESTDIR"
	BINDIR="$DESTDIR$PREFIX/bin"
	ln -sf pkgconf "$BINDIR/pkg-config"
	ln -sf pkgconf "$BINDIR/$TARGET-pkgconf"
	ln -sf pkgconf "$BINDIR/$TARGET-pkg-config"

	# build the personality
	mkdir -p "$DESTDIR$PREFIX/share/pkgconfig/personality.d"
	echo "Triplet: $HOST
DefaultSearchPaths: /usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig
SystemIncludePaths: /usr/include:/usr/local/include
SystemLibraryPaths: /usr/lib:/usr/local/lib" > "$DESTDIR$PREFIX/share/pkgconfig/personality.d/$TARGET.personality"
}
```
