#OUTPUT_FORMAT(elf64-aarch64)
OUTPUT_ARCH(aarch64)

ENTRY(_start)
PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
    dynamic PT_DYNAMIC FLAGS((1 << 1) | (1 << 2)) ; /* Dynamic PHDR for relocations */
}

SECTIONS
{
    . = 0xffffffff80000000;
    p_kernel_start = .;
    .text : {
        *(.text .text.*)
    } :text
    . += CONSTANT(MAXPAGESIZE);
    p_kernel_text_end = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    . += CONSTANT(MAXPAGESIZE);

    .data : {
        *(.data .data.*)
    } :data

    .dynamic : {
        *(.dynamic)
    } :data :dynamic

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data
    . += CONSTANT(MAXPAGESIZE);
    p_kernel_end = .;
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    } 
}