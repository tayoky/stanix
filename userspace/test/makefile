MAKEFLAGS += --no-builtin-rules

#first get all the src
SRC_C = $(shell find -name "*.c")
SRC_ASM = $(shell find -name "*.s")
OBJ = ${SRC_C:.c=.o} ${SRC_ASM:.s=.o}

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename ${PWD})

#util
CC = gcc
LD = ld
NASM = nasm

#ld flags
LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
#nasm flags
ASMFLAGS += -Wall \
    -f elf64

#cc flags
CFLAGS = -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
CFLAGS += -I ./

include ../${ARCH}.mk

all : ${OUT}

${OUT} : ${OBJ}
	${LD} ${LDFLAGS} -o ${OUT} ${OBJ}

%.o : %.c
	${CC} ${CFLAGS} -o $@ -g -c $^
%.o : %.s
	${NASM} ${ASMFLAGS} $< -o $@

clean : 
	rm -f ${OBJ}