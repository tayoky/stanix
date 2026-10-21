---
title: stanix-games
---

# Description
version : unknown  

# Dependencies
- [stanix-base](stanix-base)
- [doom-shareware](doom-shareware)
- [quake2-shareware](quake2-shareware)
- [nyancat](nyancat)
- [infones](infones)

# Install
To build and install this package, run  
```sh
./tinx.sh install stanix-games
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh stanix-games
```

# Manifest
```sh
# a meta packages with every game on stanix
DEPENDENCIES="stanix-base doom-shareware quake2-shareware nyancat infones"
. "$TOP/scripts/meta-package.sh"
```
