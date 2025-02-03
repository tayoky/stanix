#output
include out.mk

#define your compilator and other tools here
CC = gcc
LD = ld
NASM = nasm

#flag
LDFLAGS = 
ASMFLAGS =

#all sources
src_c = $(shell find -name "*.c")
src_asm = $(shell find -name *.s)
OBJ = ${src_c:.c=.o} ${src_asm:.s=.o}

#DO NOT EDIT
#except if you know what you are doing
LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T linker.ld
ASMFLAGS += -Wall \
    -f elf64
include include.mk

${out}/boot/${kernel} : ${OBJ}
	${LD} ${LDFLAGS} ${OBJ} -o ${out}/boot/${kernel}

${out}/boot/limine/limine.conf : limine.conf
	cp limine.conf ${out}/boot/limine/limine.conf
%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
%.o : %.s
	${NASM} ${ASMFLAGS} $< -o $@
clean :
	rm ${OBJ} ${out}