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
- `pkg-config`

### Required software for cross-compiling
If cross-compiling from an OS other than Stanix, you will also need the following software to build a **cross-compiler**.
- a C and C++ compiler (e.g. `gcc` and `g++`)
- `MPC` (libmpc-dev)
- `GMP` (libgmp-dev)
- `MPFR` (libmpfr-dev)
- `bison`
- `flex`
- `texinfo`
- `automake`
- `autoconf` (2.71 or change version in the script)
- a shell (e.g. `sh`, `bash`, `dash`, `ksh`, ...)
- `coreutils`
- `make`

On Ubuntu you can get everything by running
```sh
sudo apt install build-essential bison flex texinfo libmpc-dev libgmp-dev libmpfr-dev automake autoconf
```

## Cloning
First, clone the repository.
```sh
git clone https://github.com/tayoky/stanix --recurse-submodules
cd stanix
```

## Configure
Then, run the configure script. It will detect whether you are cross-compiling and build the toolchain if needed.
```sh
./configure --host="x86_64-stanix"
```
See [configuration options](#configuration-options) for all supported options

> [!NOTE]
> Compiling the cross toolchain the first time (that contains GCC) may take some time.

> [!NOTE]  
> Supported values for `--host` are `x86_64-stanix`, `i386-stanix` and `aarch64-stanix` 

## Compiling
Run `make build-all` and then run `make image-all` for all images or
- `make image-hdd` For hdd image.
- `make image-iso` For iso image.
- `make test-qemu` Create an hdd image and automatically launch it with QEMU.
Do `make targets` to get a list of all targets.

## Configuration options
All options supported by the `./configure` script.
- `--cc=CC` Set the C compiler.
- `--cxx=CXX` Set the C++ compiler.
- `--as=AS` Set the assembler.
- `--ar=AR` Set the archiver.
- `--ld=LD` Set the linker.
- `--objcopy=OBJCOPY` Set the object copy utility.
- `--strip=STRIP` Set the stripper.
- `--nm=NM` Set the nm.
- `--pkgconfig=PKGCONFIG` Set the pkg-config.
- `--cflags=CFLAGS` Set custom flags for C compilation.
- `--cxxflags=CXXFLAGS` Set custom flags for C++ compilation.
- `--asflags=ASFLAGS` Set the flags for assembling.
- `--arflags=ARFLAGS` Set the flags for archiving.
- `--ldflags=LDFLAGS` Set the flags for linking.
- `--host=HOST` Set the OS to build, this should always be `x86_64-stanix`, `i386-stanix`, `aarch64-stanix` or omitted.
- `--build=BUILD` Set the build OS, can be set if tconf cannot determine the build OS.
- `--clear-cache` Clear the cache before doing anything.
- `--prefix=PREFIX` Set the prefix.
- `--sysroot=SYSROOT`, `--with-sysroot=SYSROOT` Set the sysroot.
- `--debug` Compile with debug options activated.
- `--help` Show help and exit.
- `--enable-cross-toolchain` Force the configure script to build a cross toolchain, even when building from Stanix.
- `--disable-cross-toolchain` Force the configure script to not build a cross toolchain, even when cross-compiling (can be used to use a custom cross-compiler).

## Installing programs
For now, any program you want to install must be put into `./initrd/bin/` in the repo.  
Then redo `make build-initrd` and `make image-hdd`.  

You can install a port by entering into the `ports` directory then running `./build.sh XXX` and `./install.sh XXX`.
Example with Doom :
```sh
cd ports
./build.sh doomgeneric
./install.sh doomgeneric
```

A list of ports is available [here](https://tayoky.github.io/stanix/packages).
