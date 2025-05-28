# build
there are two possiblity : 
- you are building StanixOS on itself (self building)
- you are building StanixOS form another OS (eg linux/gnu or any other unix like OS)
## cross building
if you are building from another OS you will first have to make an cross compiler  
### required sofware
- git
- gcc or any c compilator
- coreutils
- a shell
- make

firt clone the repo
```sh
git clone https://github.com/tayoky/stanix --recurse
cd stanix
```
then you can run the script to build the toolchain
```sh
./build-toochain.sh --target=x86_64-stanix
```
> [!NOTE]
> other values for `--target` include `i386-stanix` and `aarch64-stanix` in function of what architecture you want to build for 

now source the script to setup the `PATH` `CC` ,... variables
```sh
. ./add-to-path.sh
```
> [!NOTE]
> you will have to do this step every time
you quit your sessions and come back (or open/close a new terminal window)   
> once you have done this step the default C compiler for this sessions is a stanix cross compiler use `unset CC` to go back to your old compiler

then you can follow the guide same as [self building](#self-building)
.
## self building
### required software
- git
- tcc (or gcc and ld)
- make
- nasm
- coreutil
- gdisk
- mtools (for hdd imaages)
- xorriso (for iso images)
first configure
```sh
./configure --host=x86_64-stanix
```
> [!NOTE]
> other values for `--host` include `i386-stanix` and `aarch64-stanix` in function of what architecture you want to build for

see [configuration option](#configuration-options) for all supported options  
then just run `make all` for all images or
- `make hdd` for hdd image
- `make iso` for iso image
`make test` create an hdd image  and automaticly launch it with qemu
## configuration options
all options supported by the `./configure` script
- `--host=XXX` precise the host for finding the compilator this should always be `x86_64-stanix` or not present
- `--with-sysroot=XXX` precise a custom sysroot path
- `--cc=XXX` use a custom c compiler
- `--ld=XXX` use a custom linker
- `--nasm=XXX` use a custom assembler NOTE : the assembler must use the intel syntax and make 64 bits objects files
# installing programs
for the moment, any program you want to install must be put into `./initrd/bin/` in the repo  
then redo `make all`  

you can install port by going into the ports folder then running `./build.sh XXX` and `./install.sh XXX`
exemple with doom :
```sh
cd ports
./build.sh doomgeneric
./install.sh doomgeneric
```

a list of ports is available [here](https://tayoky.github.io/stanix/packages)
