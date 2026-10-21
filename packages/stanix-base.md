---
title: stanix-base
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/base/sysroot`  

# Dependencies
- [stanix-kernel](stanix-kernel)
- [stanix-modules](stanix-modules)
- [stanix-libraries](stanix-libraries)
- [stanix-userspace](stanix-userspace)
- [tlibc](tlibc)
- [tash](tash)
- [tutils](tutils)
- [tvi](tvi)
- [tgui-apps](tgui-apps)
- [limine-files](limine-files)
- [libgcc_s](libgcc_s)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-base
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-base
```

# Manifest
```sh
SOURCE="stanix-base"
DEPENDENCIES="stanix-kernel stanix-modules stanix-libraries stanix-userspace 
tlibc tash tutils tvi tgui-apps
limine-files libgcc_s"
. "$TOP/scripts/stanix-package.sh"

configure () {
	true
}

build () {
	true
}

install () {
	# install base sysroot
	mkdir -p "$DESTDIR"
	mkdir -p "$DESTDIR/dev" "$DESTDIR/tmp" "$DESTDIR/mnt" "$DESTDIR/proc" "$DESTDIR/sys"
	cp -Pf -rp "$SOURCE_DIR"/* "$DESTDIR/"

	# install os release
	"$TOP/scripts/generate-os-release.sh" > "$DESTDIR/etc/os-release"
}
```
