![](https://tokei.rs/b1/github/tayoky/FOS2) ![GitHub top language](https://img.shields.io/github/languages/top/tayoky/FOS2)
   
# StanixOS
An 64 bit OS made from scratch  
this os use the stanix kernel an 64 bit monolic higher half kernel  
[credit here](CREDITS.md)
## build
to build an image of the os you first need to make sure to have all of this :  
- git
- mtools (only for hdd images)
- nasm
- gcc
- ld
- xorriso (only for iso images)
- qemu (for testing in vm ot any other vm)
- make
### submodule
the repo as two submodules : limine and tlibc   
clone the submodules :  
```sh
git submodule init
git submodule update
```
### building
see the [build docs](BUILD.md)
