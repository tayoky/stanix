---
title: build
---
There are two possibilities : 
- You are building Stanix on itself (**self building**).
- You are building Stanix from another OS (e.g. GNU/Linux or any other Unix-like OS) (**cross-building**).
In both cases the process is similar.

> [!NOTE]
> If building from Windows, use WSL.

## Required software
To build Stanix, you need the following software.
- `git`
- `gcc` `ld` `ar` and `as` (`tcc`, or any other C compiler with GNU extensions, might work, but is untested)
- `make`
- `nasm`
- `coreutils`
- `gdisk` (for hdd images)
- `mtools` (for hdd images)
- `xorriso` (for iso images)

### Required software for cross-compiling
If cross-compiling from an OS other than Stanix, you will also need the following software to build a **cross-compiler**.
- a C and C++ compiler (e.g. `gcc` and `g++`)
- `MPC` (libmpc-dev)
- `GMP` (libgmp-dev)
- `MPFR` (libmpfr-dev)
- `bison`
- `flex`
- `texinfo`
- a shell (e.g. `sh`, `bash`, `dash`, `ksh`, ...)
- `coreutils`
- `make`

On Ubuntu you can get everything by running
```sh
sudo apt install build-essential bison flex texinfo libmpc-dev libgmp-dev libmpfr-dev nasm xorriso gdisk mtools
```

## Cloning
First, clone the repository.
```sh
git clone https://github.com/tayoky/stanix --recurse-submodules
cd stanix
```

## Compiling
Run `./tinx.sh --host=x86_64-stanix install` and then run :
- `./tinx.sh run scrupts/build-hdd-image.sh` For hdd image.
- `./tinx.sh run scripts/build-iso-image.sh` For iso image.
- `./tinx.sh run scripts/qemu.sh` To test the image in QEMU.

> [!NOTE]  
> Compiling the cross toolchain the first time (that contains GCC) may take some time.

> [!NOTE]  
> Supported values for `--host` are `x86_64-stanix`, `i386-stanix` and `aarch64-stanix` 

> [!NOTE]  
> Generated images can be found in `build/images`

> [!NOTE]
> Do `./tinx.sh run scripts/qemu.sh --help` to see every options of the QEMU wrapper.

## Installing programs
Any program you want to install must be installed into `build/sysroot`.  
You can install most packages by doing
```sh
./tinx.sh --host=x86_64-stanix package-name
```

For exemple to install the Doom shareware you can do
```sh
./tinx.sh --host=x86_64-stanix doom-shareware
```

Then you can rebuild the image.

A list of ports is available [here](https://tayoky.github.io/stanix/packages).
