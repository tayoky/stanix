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

first create an sysroot with the libc header : 
```sh
cd stanix
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
you can folow the rest of the tutorial like on stanix self building
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
just run `make all HOST=x86_64-stanix` for all images or
- `make hdd HOST=x86_64-stanix` for hdd image
- `make iso HOST=x86_64-stanix` for iso image
`make test` create an hdd image for x86_64 and automticly launch it with qemu