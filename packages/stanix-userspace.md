---
title: stanix-userspace
---

# Description
version : 0.2.1-indev  
website : [https://tayoky.github.io/stanix](https://tayoky.github.io/stanix)  
dir     : `${STANIX_REPO}/userspace`  

# Dependencies
- [tlibc](tlibc)
- [stanix-libraries](stanix-libraries)
- [stanix-modules](stanix-modules)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-userspace
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-userspace
```

# Manifest
```sh
SOURCE="stanix-userspace"
BUILD_DEPENDENCIES="stanix-toolchain limine-binaries"
DEPENDENCIES="tlibc stanix-libraries stanix-modules"
. "$TOP/scripts/stanix-package.sh"
```
