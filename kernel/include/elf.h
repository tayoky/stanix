#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <elf.h>

#ifdef __i386__
#define Elf_Ehdr      Elf32_Ehdr
#define Elf_Phdr      Elf32_Phdr
#define Elf_Shdr      Elf32_Shdr
#define Elf_Addr      Elf32_Addr
#define Elf_Sym       Elf32_Sym
#define Elf_Rela      Elf32_Rela
#define ELF_R_SYM(i)  ELF32_R_SYM(i)
#define ELF_R_TYPE(i) ELF32_R_TYPE(i)
#define ELF_R_INFO(i) ELF32_R_INFO(i)
#define ELFCLASS      ELFCLASS32
#else
#define Elf_Ehdr      Elf64_Ehdr
#define Elf_Phdr      Elf64_Phdr
#define Elf_Shdr      Elf64_Shdr
#define Elf_Addr      Elf64_Addr
#define Elf_Sym       Elf64_Sym
#define Elf_Rela      Elf64_Rela
#define ELF_R_SYM(i)  ELF64_R_SYM(i)
#define ELF_R_TYPE(i) ELF64_R_TYPE(i)
#define ELF_R_INFO(i) ELF64_R_INFO(i)
#define ELFCLASS      ELFCLASS64
#endif

#ifdef __i386__
#define ELFMACHINE EM_386
#define ELFDATA ELFDATA2LSB
#elif defined(__x86_64__)
#define ELFMACHINE EM_X86_64
#define ELFDATA ELFDATA2LSB
#elif defined(__aarch64__)
#define ELFMACHINE EM_AARCH64
#define ELFDATA ELFDATA2LSB
#else
#error "unknown ELF_MACHINE for current architecture"
#endif

#endif
