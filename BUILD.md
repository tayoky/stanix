# bulding
to build build StanixOS there are two possibility  
- you currently run StanixOS (problably not)
- you want to compile StanixOS from another host OS 
## cross build
if you want yo compile StanixOS from another host OS you will first need to install required sofware
### required sofware
you need to install all of this (`sudo apt install xxx` on ubuntu)  
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
### cross compilator
you then need to make a cross compilator and then you can follow the build instructions like you were on stanix
## build from StanixOS
if you build on StanixOS (or any other OS  with cross compilator) you first need to install all of this sofware
### required software
- git
- make
- gcc (or cross one if cross compiling)
- binutil (or cross one if cross compiling)
- nasm
- xorriso (for iso images)
- mtools (for hdd image)
- qemu or any other VM (recommanded for testing)
###cloning
now clone the repo 
```sh
git clone https://github.com/tayoky/FOS2
```
and it's submodules
```sh
git submodule init
git submodule update
```
###building
now you can actually build  
NOTE : if you're using a cross compilator change CC and LD to your linker and compilator in kernel/makefile
  
firt step : make tlibc and install it into sysroot
```sh
make -C tlibc
cd tlibc && ./install.sh ../sysroot
```
now you can build the userspace and install it
```sh
make -C userspace install
```
NOTE : the userspace for the moment don't support any other sysroot
then build the kernel, the ramdisk and the boot directory 
```sh
make
```
now you can make your own image with sysroot and out or do  
- `make iso`
- `make hdd`  
  
`make test` will automacily launch qemu with and hdd image 