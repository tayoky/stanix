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
## self building
### required software
- git
- gcc
- ld
- make
- nasm
- coreutil
just run `make all HOST=x86_64-stanix` for all images or
- `make hdd HOST=x86_64-stanix` for hdd image
- `make iso HOST=x86_64-stanix` for iso image
`make test` create an hdd image for x86_64 and automticly launch it with qemu