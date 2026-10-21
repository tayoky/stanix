---
title: stanix-dev
---

# Description
version : unknown  

# Dependencies
- [stanix-base](stanix-base)
- [make](make)
- [tcc](tcc)
- [binutils](binutils)
- [nasm](nasm)
- [pkgconf](pkgconf)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-dev
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-dev
```

# Manifest
```sh
# a meta package to install everything needed to do dev from stanix itself
DEPENDENCIES="stanix-base make tcc binutils nasm pkgconf"
. "$TOP/scripts/meta-package.sh"
```
