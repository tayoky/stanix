---
title: stanix-full
---

# Description
version : unknown  

# Dependencies
- [stanix-base](stanix-base)
- [stanix-dev](stanix-dev)
- [stanix-games](stanix-games)
- [mtools](mtools)
- [ffmpeg](ffmpeg)
- [mesa](mesa)
- [libarchive](libarchive)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-full
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-full
```

# Manifest
```sh
# a meta package to install everything in a full Stanix distro
DEPENDENCIES="stanix-base stanix-dev stanix-games mtools ffmpeg mesa libarchive"
. "$TOP/scripts/meta-package.sh"
```
