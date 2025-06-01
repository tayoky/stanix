
#first get all the src
SRC_C = $(shell find -name "*.c")
SRC_ASM = $(shell find -name "*.s")
OBJ = ${SRC_C:.c=.o} ${SRC_ASM:.s=.o}

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename ${PWD})

#ld flags
LDFLAGS += \
    -nostdlib \
    -static \
#nasm flags
ASMFLAGS += -Wall \
    -f elf64

#cc flags
CFLAGS = -Wall \
    -Wextra \
    -std=c11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC
CFLAGS += -I ./

all : ${OUT}

${OUT} : ${OBJ}
	${CC} ${LDFLAGS} -o ${OUT} ${OBJ}

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
%.o : %.s
	${NASM} ${ASMFLAGS} $< -o $@

clean : 
	rm -f ${OBJ}