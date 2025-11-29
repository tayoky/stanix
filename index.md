---
title: stanix
---
stanix is a 64 bits open source operating system made by tayoky since december 2024  
the entire operating system is written from scratch with its own libc kernel shell and userspace

## download
a new stable release is avalible each month
- [source code](https://github.com/tayoky/stanix)
- [lasted release](https://github.com/tayoky/stanix/releases)
- [compile from source](miscellaneous/build)

## hardware
list of all supported hardware  
- serial port (only on `x86_64`)
- ps2 keyboard (only on `x86_64`)
- cmos (only on `x86_64`)
- framebuffer (with tty emulation)
- ATA devices on ide controller
- pci bus

## recommanded specs
- at least 256 mb of ram (also run on 128 mb)
- 10mhz `x86_64` one core cpu (`aarch64` is comming in a few release)
- ps2 keyboard
- serial port (optional just make debugging easier
)

## features
- vfs
- tmpfs
- userspace
- multithreading
- pipes
- pty system
- usermode
- multi user
- dynamic module loading
- fat32 support
- a pretty complete libc with over 300 functions
- somes ports (such as doom,binutils,...)

## drivers list
- 8042 (ps2 controller on most PC)
- ATA
- framebuffer
- serial (early boot)
- serial (complete driver)
- pci bus
- ps2 keyboard
- partitions (GPT/MBR)
- fat12/16/32 (readonly)

## packages
a list of availables packages and ports can be found [here](packages)

## docs
see [kernel docs](kernel) and [userspace docs](user)  

