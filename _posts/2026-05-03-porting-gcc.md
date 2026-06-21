---
title: porting GCC
---
One of the main goal of Stanix always as been **self-hosting**, and while TCC is a really cool compiler that can compile most of Stanix's userspace it has multiples issues that make it non suitable for full self hosting:
- TCC does not optimize well
- TCC lack stdatomic
- TCC lack some features used by the kernel
- TCC cannot compile C++ (required for some ports)

In order to fix these, another compiler must be used, the compiler currently used to cross compile Stanix, GCC. Specially GCC 12.2.0 (the version used to cross compile Stanix as of this writing). GCC is much harder to port than TCC as it has many dependencies (`binutils`, `libgmp`, `libmpfr` and `libmpc`) and is written in C++, which add an additional dependency to `libstdc++`. The first step to port GCC was to cross compile it's dependencies.

## cross compiling dependencies

### binutils
`binutils` was already ported since quite a long time and is really stable on Stanix.

## libgmp
`libgmp` is when the actual work begin, I first downloaded `libgmp` 6.3.0 patched the `configfsf.sub` (yeah not `config.sub` this time) and cross compiled. Then something unexpected happened during compilation :
```
inp_str.c: In function '__gmpz_inp_str':
../gmp-impl.h:1782:29: warning: implicit declaration of function '__gmpz_inp_str_nowhite'; did you mean 'mpz_inp_str_nowhite'? [-Wimplicit-function-declaration]
 1782 | #define mpz_inp_str_nowhite __gmpz_inp_str_nowhite
      |                             ^~~~~~~~~~~~~~~~~~~~~~
inp_str.c:63:10: note: in expansion of macro 'mpz_inp_str_nowhite'
   63 |   return mpz_inp_str_nowhite (x, stream, base, c, nread);
      |          ^~~~~~~~~~~~~~~~~~~
inp_str.c: At top level:
../gmp-impl.h:1782:29: error: conflicting types for '__gmpz_inp_str_nowhite'; have 'size_t(__mpz_struct *, FILE *, int,  int,  size_t)' {aka 'long unsigned int(__mpz_struct *, FILE *, int,  int,  long unsigned int)'}
 1782 | #define mpz_inp_str_nowhite __gmpz_inp_str_nowhite
      |                             ^~~~~~~~~~~~~~~~~~~~~~
inp_str.c:68:1: note: in expansion of macro 'mpz_inp_str_nowhite'
   68 | mpz_inp_str_nowhite (mpz_ptr x, FILE *stream, int base, int c, size_t nread)
      | ^~~~~~~~~~~~~~~~~~~
../gmp-impl.h:1782:29: note: previous implicit declaration of '__gmpz_inp_str_nowhite' with type 'int()'
 1782 | #define mpz_inp_str_nowhite __gmpz_inp_str_nowhite
      |                             ^~~~~~~~~~~~~~~~~~~~~~
inp_str.c:63:10: note: in expansion of macro 'mpz_inp_str_nowhite'
   63 |   return mpz_inp_str_nowhite (x, stream, base, c, nread);
      |          ^~~~~~~~~~~~~~~~~~~
/bin/bash ../libtool  --tag=CC   --mode=compile x86_64-stanix-gcc -DHAVE_CONFIG_H -I. -I..  -D__GMP_WITHIN_GMP -I..   -O2 -g -c -o iset_si.lo iset_si.c
make[2]: *** [Makefile:488: inp_str.lo] Error 1
make[2]: *** Waiting for unfinished jobs....
```
Uh `__gmpz_inp_str_nowhite` is not defined, this is really weird. So i then recompiled the exact same package on my host (a linux mint) and it worked perfectly... Weird. I then looked at the file `gmp_impl.h` and found out this :
```c
#define mpz_inp_str_nowhite __gmpz_inp_str_nowhite
#ifdef _GMP_H_HAVE_FILE
__GMP_DECLSPEC size_t  mpz_inp_str_nowhite (mpz_ptr, FILE *, int, int, size_t);
#endif
```
For some reason the declaration was behind a check for a feature. As suggested by `cpplover0` on the osdev discord i checked the `config.log` for anything related to some kind of `HAVE_FILE` and found nothing. Until a `grep -r` finally told me where was this check done, in `gmp-h.in` :
```c
/* For reference, "defined(EOF)" cannot be used here.  In g++ 2.95.4,
   <iostream> defines EOF but not FILE.  */
#if defined (FILE)                                              \
  || defined (H_STDIO)                                          \
  || defined (_H_STDIO)               /* AIX */                 \
  || defined (_STDIO_H)               /* glibc, Sun, SCO */     \
  || defined (_STDIO_H_)              /* BSD, OSF */            \
  || defined (__STDIO_H)              /* Borland */             \
  || defined (__STDIO_H__)            /* IRIX */                \
  || defined (_STDIO_INCLUDED)        /* HPUX */                \
  || defined (__dj_include_stdio_h_)  /* DJGPP */               \
  || defined (_FILE_DEFINED)          /* Microsoft */           \
  || defined (__STDIO__)              /* Apple MPW MrC */       \
  || defined (_MSL_STDIO_H)           /* Metrowerks */          \
  || defined (_STDIO_H_INCLUDED)      /* QNX4 */                \
  || defined (_ISO_STDIO_ISO_H)       /* Sun C++ */                \
  || defined (__STDIO_LOADED)         /* VMS */                        \
  || defined (_STDIO)                 /* HPE NonStop */         \
  || defined (__DEFINED_FILE)         /* musl */
#define _GMP_H_HAVE_FILE 1
#endif
```
Okay this check is extremely weird but it make sense it does not work. For some reason `libgmp` thought it would be a good idea to check for libc specific **header guard** to see if `<stdio.h>` was in fact included correctly. On Stanix, [tlibc](https://github.com/tayoky/tlibc) is used as libc, and tlibc used `STDIO_H` as header guard which was not checked for. So i updated all header of tlibc to header guards prefixed with an underscore. I recompiled and BINGO ! `libgmp` was finally ready.

## libmpfr
I cross compiled `libmpfr` 4.2.2 same as `libgmp` and it worked first try. Nothing really interesting here.

## libmpc
I cross compiled `libmpc` 1.4.4 same as `libgmp` and it got pretty far in the compilation, but during linking this happened :
```
/bin/bash ../libtool  --tag=CC   --mode=link x86_64-stanix-gcc  -O2 -g  -version-info 7:1:4  -o libmpc.la -rpath /usr/lib abs.lo acos.lo acosh.lo add.lo add_fr.lo add_si.lo add_ui.lo agm.lo arg.lo asin.lo asinh.lo atan.lo atanh.lo clear.lo cmp.lo cmp_abs.lo cmp_si_si.lo conj.lo cos.lo cosh.lo div_2si.lo div_2ui.lo div.lo div_fr.lo div_ui.lo dot.lo eta.lo exp.lo fma.lo fr_div.lo fr_sub.lo get_prec2.lo get_prec.lo get_version.lo get_x.lo imag.lo init2.lo init3.lo inp_str.lo log.lo log10.lo mem.lo mul_2si.lo mul_2ui.lo mul.lo mul_fr.lo mul_i.lo mul_si.lo mul_ui.lo neg.lo norm.lo out_str.lo pow.lo pow_fr.lo pow_ld.lo pow_d.lo pow_si.lo pow_ui.lo pow_z.lo proj.lo real.lo rootofunity.lo urandom.lo set.lo set_prec.lo set_str.lo set_x.lo set_x_x.lo sin.lo sin_cos.lo sinh.lo sqr.lo sqrt.lo strtoc.lo sub.lo sub_fr.lo sub_ui.lo sum.lo swap.lo tan.lo tanh.lo uceil_log2.lo ui_div.lo ui_ui_sub.lo radius.lo balls.lo exp10.lo exp2.lo log2.lo  -lmpfr -lgmp 
/usr/bin/grep: /usr/lib/libgmp.la: No such file or directory
/usr/bin/sed: can't read /usr/lib/libgmp.la: No such file or directory
libtool:   error: '/usr/lib/libgmp.la' is not a valid libtool archive
make[2]: *** [Makefile:486: libmpc.la] Error 1
make[2]: Leaving directory '/home/tayoky/git-repo/stanix/ports/tar/libmpc/src'
make[1]: *** [Makefile:510: all-recursive] Error 1
make[1]: Leaving directory '/home/tayoky/git-repo/stanix/ports/tar/libmpc
```
This is weird, it's searching for `libgmp` on my host system, outside the sysroot. Well i spend an hour trying to combine flags to get it to work until i looked at the osdev wiki about `.la` files. Basically they are a broken alternative to **pkg-config** files and are not sysroot aware. Which mean cross compiling with them is really painful and they aren't needed as `libgmp` and most libraries provide **pkg-config** files anyway. So as suggested by the osdev wiki, I made the port builder delete all `.la` files after installations. And it WORKED ! Now all three math libraries were ready.

## libstdc++-v3
Compiling `libstdc++-v3` is much weirder than compiling standard independent libs, as `libstdc++-v3` is a direct part of GCC. So I had to modify my `build-toolchain.sh` script to enable `libstdc++-v3` which was a pretty hard to do and required modifying my patches. Once this was done I had to be very careful to build things in this order :
- main GCC cross compiler first `all-gcc` (needed to compile the rest)
- then `libgcc` `all-target-libgcc` (needed for some parts of `tlibc`)
- then `tlibc` as the standard c++ library depends on it
- and finally `libstdc++-v3`

Once I reached the compile `libgcc` step the installation always complain about a missing `libgcc_eh.h` but it seem to work without it. Finally when the standard c++ library compiled I was rewarded with a bunch of errors about broken libc header and missing libc functions. I then fixed my headers guard and used the `volatile` for atomic trick recommended by `sasdallas` on the unmapped nest discord since `<stdatomic.h>` does not work well in C++. Time was then to implement various `tlibc` functions such as `strcoll` or `fgetpos`. After the standard c++ libray compiled, I tryied a quick, classic, c++ hello world. But when linking a bunch of undefined function references appeared (`_UnwindResume`, `__cxa_throw`, ...). These functions were defined in `libgcc_eh.a` (static) and `libgcc_s.so` (shared), but for some reason GCC was only picking `libgcc.a`. With `sasdallas` we then fixed the GCC patch. Then I implemented the `__cxa_atexit` and `__cxa_finalize` ABI functions and `libstdc++-v3` was finnaly working ! I also had to fix a kernel bug with the tty drover see [commit 4787752](https://github.com/tayoky/stanix/commit/4787752f6a503bfd46f6954f61f110e4ba196eab).

## GCC 12.2.0
With all depencies of GCC working mostly correcly (because of libc stubs some part of `libstdc++-v3` didn't work), it was finnaly time to port GCC itself. That how far I am currently, I will update this post when i get further.
