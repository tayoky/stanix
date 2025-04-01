# build
there are two possiblity : 
- you are building StanixOS on itself (self building)
- you are building StanixOS form another OS (eg linux/gnu or any other unix like OS)

## cross building
if you are building from another OS you will first have to make an cross compiler

### required sofware
- git
- gcc
- binutil
- make
- bison
- flex
- GMP (libgmp3-dev)
- MPC (libmpc-dev)
- MPFR (libmpfr-dev)
- textinfo
- autoconf
- automake

first clone the stanix repo and create an sysroot with the libc header : 
```sh
git clone https://github.com/tayoky/stanix --recurse
cd stanix
./configure
make header
```
the new sysroot is now avalible inside the sysroot folder in the repo

### cloning
you now have to clone the patched version of gcc and binutils 
```sh
git clone https://github.com/tayoky/binutils-gdb.git
git clone https://github.com/tayoky/gcc.git
```

### configure
now we have to configure some variables
```sh
export SYSROOT=/path/to/sysroot
export PREFIX=$HOME/cross/stanix
export PATH=$PREFIX/bin:$PATH
export TARGET=x86_64-stanix
```

### build
now build binutils and gcc  
binutils :
```sh
cd binutils-gdb
./configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --disable-werror
make
make install
```

gcc :
```sh
cd gcc
./configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c,c++  --with-sysroot=$SYSROOT --disable-hosted-libstdcxx
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc
```

you can folow the rest of the tutorial like on [stanix self building](#self-building)

## self building

### required software
- git
- gcc
- ld
- make
- nasm
- coreutil
- gdisk
- mtools

first configure
```sh
./configure
```
NOTE : if you are cross compiling stanix you need the host option : 
```sh
./configure --host="x86_64-stanix"
```
see [configuration option](#configuration-options) for all supported options  
then just run `make all` for all images or
- `make hdd` for hdd image
- `make iso` for iso image
`make test` create an hdd image  and automaticly launch it with qemu

## configuration options
all options supported by the `./configure` script
- `--host=XXX` precise the host for finding the compilator this should always be `x86_64-stanix` or not present
- `--with-sysroot=XXX` precise a custom sysroot path
- `--with-CC=XXX` use a custom c compiler
- `--with-LD=XXX` use a custom linker
- `--with-NASM=XXX` use a custom assembler NOTE : the assembler must use the intel syntax and make 64 buts objects files

# installing programs
for the moment, any program you want to install must be put into `./initrd/bin/` in the repo  
then redo `make all`  
NOTE : there is currently no list of port (the only one being the very broken port of doomgeneric) the list will be made later (i will update this guide when it happen)
