# build
there are two possiblity : 
- you are building Stanix on itself (**self building**)
- you are building Stanix form another OS (eg linux/gnu or any other unix like OS) (**cross building**)

> [!NOTE]
> if building from window use WSL
## cross building
if you are building from another OS you will first have to make an **cross toolchain**
 
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

on ubuntu you can get everything by
```sh
sudo apt install git build-essential bison flex texinfo libmpc-dev libgmp-dev libmpfr-dev automake autoconf
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
./build-toolchain.sh --target=x86_64-stanix
```
> [!NOTE]  
> do `./build-toolchain.sh --help`  
> for a full list of the options

> [!NOTE]  
> supported values for `--target` are `x86_64-stanix`, `i386-stanix` and `aarch64-stanix` 

now source the script to setup the `PATH` `CC` ,... variables
```sh
. ./add-to-path.sh
```
> [!NOTE]  
> you will have to do this step every time
you quit your sessions and come back (or open/close a new terminal window)   
> once you have done this step the default C compiler for this session is a stanix cross compiler use `unset CC` to revert to your system compiler

then you can follow the guide same as [self building](#self-building)
.
## self building
if you are compiling from sanix (or using the cross toolchain) follow these steps

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

see [configuration option](#configuration-options) for all supported options  

### compiling
first run `make` and then `./essential-ports.sh` to build tsh and tutils 
then just run `make all` for all images or
- `make hdd` for hdd image
- `make iso` for iso image
- `make test` to create an hdd image and automaticly launch it with qemu
- `make cdrom-test` to create an iso image and automaticly launch it with qemu

## configuration options
all options supported by the `./configure` script
- `--host=XXX` precise the host for finding the compiler this should always be `XXX-stanix` or not present
- `--sysroot=XXX` custom sysroot path
- `--cc=XXX` use a custom c compiler
- `--ld=XXX` use a custom linker
- `--ar=XXX` use a custom archiver
- `--as=XXX` use a custom AT&T syntax assembler
- `--nm=XXX` use a custom nm (usefull if it try tu use llvm-nm for some reason)
- `--nasm=XXX` use a custom intel syntax assembler (must produce 64-bit objects files)

# installing programs
for the moment, any program you want to install must be put into `initrd/bin/` in the repo  
then redo `make all`  

you can install port by going into the ports folder then running `./build.sh XXX` and `./install.sh XXX`
exemple with doom :
```sh
cd ports
./build.sh doomgeneric
./install.sh doomgeneric
```

a list of ports is available [here](https://tayoky.github.io/stanix/packages)
