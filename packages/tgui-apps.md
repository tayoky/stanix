---
title: tgui-apps
---

# Description
version : unknown  
git     : [https://github.com/stanix-org/tgui-apps](https://github.com/stanix-org/tgui-apps)  
commit  : c75e8492749b1cdad4377b535656b6504b6cc38f  

# Dependencies
- [stanix-libraries](stanix-libraries)

# Install
To build and install this package, run  
```sh
./tinx.sh install tgui-apps
```

Alternatively you can try to download pre compiled of this package using
```sh
./tinx.sh run scripts/download-packages.sh tgui-apps
```

# Manifest
```sh
SOURCE="tgui-apps"
BUILD_DEPENDENCIES="stanix-toolchain"
DEPENDENCIES="stanix-libraries"
. "$TOP/scripts/tconf-package.sh"
PREFIX="/usr"
```
