---
title: Stanix
layout: page
---
Stanix is a 64-bit open source operating system made from scratch started in December 2024.  
The entire operating system is written from scratch with its own libc kernel shell and userspace (only a third-party bootloader called "Limine" is used)
Stanix aim to be Unix-like (not a Unix clone, some things are different) with the required Posix functions to port programs.

## Download
A new stable release is made sometimes...  
- [source code](https://github.com/tayoky/stanix)
- [lasted release](https://github.com/tayoky/stanix/releases)
- [compile from source](miscellaneous/build)

## Hardware
List of all supported hardware.
- serial port (only on `x86_64`)
- ps2 keyboard (only on `x86_64`)
- cmos (only on `x86_64`)
- framebuffer (with tty emulation)
- ATA devices on ide controller
- pci bus

## Goals
Futures goals (once a goal is reached it is removed from the list).
- a GUI (in progress)
- a SDL3/2 port
- a MESA port (in progress)
- nvme support
- atapi support
- better blockfs interface at kernel level
- ext2 support
- fat write support
- cirrus logic driver
- automatic modesetting
- better abstraction and management of framebuffers

## Recommended specs
- at least 256 MB of ram (also run on 128 MB)
- 10mhz `x86_64` one core cpu 
- ps2 keyboard
- serial port (optional just make debugging easier
)

## Features
- vfs
- tmpfs
- userspace
- multithreading
- pipes
- pty system
- usermode
- signals
- socket stack
- a window manager
- Unix socket
- multi user
- shared memory system
- dynamic module loading
- bus management system with hot-plug support
- fat32 support
- a pretty complete libc with over 300 functions
- some ports (such as Doom, `binutils`, `make`, ...)
- dynamic linking

## Drivers list
- 8042 (ps2 controller on most PC)
- ATA
- framebuffer
- serial (early boot)
- serial (complete driver)
- pci bus
- ps2 keyboard
- partitions (GPT/MBR)
- fat12/16/32 (read-only)
- vga
- bga

## Subprojects
**Stanix** is a big project and is split in multiple repos and projects.
- [Stanix's core](https://github.com/tayoky/stanix) contain the kernel, Stanix specific tool (terminal emulator, window manager, ...) and the base sysroot along docs.
- [tlibc](https://github.com/tayoky/tlibc) a reimplementation of the standard libc for Stanix.
- [tash](https://github.com/tayoky/tash) a homemade Posix shell used as the default one for Stanix.
- [tutils](https://github.com/tayoky/tutils) a homeade portable reimplementation of coreutils and other various generic utilities for Unix-like used for Stanix.
- [libterm](https://github.com/tayoky/libterm) a library to make terminal emulators.
- [libttf](https://github.com/tayoky/libttf) a library to parse and render truetype fonts.
- [libtgui](https://github.com/tayoky/libtgui) a GUI library with a collection of widgets.
- [Stanix's ports](https://github.com/tayoky/ports) a list of ports and packages for Stanix.

## Packages
A list of available packages and ports can be found [here](packages).

## Docs
See [kernel docs](kernel) and [userspace docs](user).

## Posts
Posts about the development of Stanix can be found [here](posts).
