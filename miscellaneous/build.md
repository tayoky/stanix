---
title: build
---
there are two possiblity : 
- you are building Stanix on itself (**self building**)
- you are building Stanix form another OS (eg linux/gnu or any other unix like OS) (**cross building**)

> [!NOTE]
> if building from window use WSL

## cross building
if you are building from another OS you will first have to make a **cross toolchain**
 
### required sofware
- `git`
- `gcc` or any c compiler
- `coreutils`
- `MPC` (libmpc-dev)
- `GMP` (libgmp-dev)
- `MPFR` (libmpfr-dev)
- `bison`
- `flex`
- `texinfo`
- `automake`
- `autoconf` (2.72 or change version in the script)
- a shell (sh,bash,dash,ksh,...)
- `make`

on ubuntu you can get everything by running
```sh
sudo apt install build-essential bison flex texinfo libmpc-dev libgmp-dev libmpfr-dev automake autoconf
```

### cloning
first clone the repo
```sh
git clone https://github.com/tayoky/stanix --recurse
cd stanix
```

### building cross toolchain
then you can run the script to build the toolchain
```sh
./build-toochain.sh --target=x86_64-stanix
```
> [!NOTE]  
> do `./build-toolchain.sh --help`  
> for a full list of the options

> [!NOTE]  
> supported values for `--target` are `x86_64-stanix`, `i386-stanix` and `aarch64-stanix` 

> [!NOTE]
> compiling the toolchain (that contain gcc) might takes some time

then you can follow the guide same as [self building](#self-building)

## self building
if you are compiling from stanix (or using the cross toolchain) follow these steps

### required software
- `git`
- `gcc` `ld` `ar` and `as` (`tcc` might also work but is untested)
- `make`
- `nasm`
- `coreutil`
- `gdisk` (for hdd images)
- `mtools` (for hdd imaages)
- `xorriso` (for iso images)
- `pkg-config`

### configure
first configure, this will detect the compiler and linker
```sh
./configure --host=x86_64-stanix
```
> [!NOTE]  
> supported values for `--host` are `x86_64-stanix`, `i386-stanix` and `aarch64-stanix` 

see [configuration options](#configuration-options) for all supported options  

### compiling
first run `make build-all` and then just run `make image-all` for all images or
- `make image-hdd` for hdd image
- `make image-iso` for iso image
`make test-qemu` create an hdd image and automaticly launch it with qemu

## configuration options
all options supported by the `./configure` script
- ``--cc=CC` set the C compiler
- ``--cxx=CXX` set the C++ compiler
- ``--as=AS` set the assembler
- ``--ar=AR` set the archiver
- ``--ld=LD` set the linker
- ``--objcopy=OBJCOPY` set the object copy utility
- ``--strip=STRIP` set the striper
- ``--nm=NM` set the nm
- ``--pkgconfig=PKGCONFIG` set the pkg-config
- ``--cflags=CFLAGS` set cutsom flags for C compilation
- ``--cxxflags=CXXFLAGS` set custom flags for C++ compilation
- ``--arflags=ASFLAGS` set the flags for assembling
- ``--asflags=ARFLAGS` set the flags for archiving
- ``--ldflags=LDFLAGS` set the flags for linking
- ``--host=HOST` set the OS to build, this should always be `x86_64-stanix` or not present
- ``--build=BUILD` set the build os, can be set if tconf cannot determinate the build os
- ``--clear-cache` clear the cache before doing anything
- ``--prefix=PREFIX` set the prefix
- ``--sysroot=SYSROOT`, `--with-sysroot=SYSROOT` set the sysroot
- ``--debug` compile with debug options actived
- ``--help` show help and exit"

# installing programs
for the moment, any program you want to install must be put into `./initrd/bin/` in the repo  
then redo `make build-initrd` and `make image-hdd`  

you can install port by going into the ports folder then running `./build.sh XXX` and `./install.sh XXX`
exemple with doom :
```sh
cd ports
./build.sh doomgeneric
./install.sh doomgeneric
```

a list of ports is available [here](https://tayoky.github.io/stanix/packages)
