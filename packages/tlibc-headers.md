---
title: tlibc-headers
---

# Description
version : 0.0.1  
git     : [https://github.com/tayoky/tlibc](https://github.com/tayoky/tlibc)  
commit  : abd22e399684fa74d53ad65b8fb18def8284ee27  

# Install
To build and install this package, run  
```sh
./tinx.sh install tlibc-headers
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tlibc-headers
```

# Manifest
```sh
VERSION="0.0.1"
SOURCE="tlibc"
. "$TOP/scripts/tconf-package.sh"

# install first party packages in /usr
PREFIX=/usr

configure () {
	# give a fake compiler since the real compiler might not be ready yet
	"$SOURCE_DIR/configure" --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX" \
		--cc=true --as=true --ar=true
}

build () {
	make all-include -j"$PARALLELISM"
}

install () {
	make install-include DESTDIR="$DESTDIR"
}
```
