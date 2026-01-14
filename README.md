![](https://tokei.rs/b1/github/tayoky/stanix) ![GitHub top language](https://img.shields.io/github/languages/top/tayoky/stanix)

# stanix
Stanix is a 64bit made from scratch operating system for the x86\_64 architecture with its own kernel, libc, terminal emulator, shell and userspace capable of running programs such as binutils, tash (homemade shell), tcc or doom.  
More informations on [the webiste](https://tayoky.github.io/stanix).

## screenshots
![a stanix screenshot with ls runnning](https://tayoky.github.io/stanix/assets/screenshot2.png)

### doom !
![doom running under stanix](https://tayoky.github.io/stanix/assets/doom.png)

## building
see the [build docs](https://tayoky.github.io/stanix/miscellaneous/build)

## subproject
While this repo contain most of Stanix's code, Stanix is made of multiples subprojects and repos.
- [Stanix's core](https://github.com/tayoky/stanix) contain the kernel, Stanix specific tool (terminal emulator, ...) and the base sysroot along docs.
- [tlibc](https://github.com/tayoky/tlibc) a reimplmentation of the standard libc for Stanix.
- [tash](https://github.com/tayoky/tash) a homemade posix shell used as the default one for Stanix.
- [tutils](https://github.com/tayoky/tutils) a homeade portable reimplementation of coreutils and other various generic utils for unix-like used for Stanix.
- [libterm](https://github.com/tayoky/libterm) a library to make terminal emulators.
- [libttf](https://github.com/tayoky/libttf) a library to parse and render truetype fonts.
- [Stanix's ports](https://github.com/tayoky/ports) a list of ports and packages for Stanix.

## contributing
Before doing any pull request ask in an issue  
as i do this mostly as a learning project i mostly do everything myself  
all commit name must be in the style `part : change` eg
- `vfs : fix vfs_mount memory leak`
- `login : ask for password before launching shell`

## credits
special thanks to [sasdallas](https://github.com/sasdallas) who helped me a lot fixing a bunch of big issues and thanks to the limine bootloader for easy higger half kernel loading.
