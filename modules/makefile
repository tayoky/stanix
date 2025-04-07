CFLAGS = -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -fsanitize=undefined
CFLAGS += --sysroot=${SYSROOT} -isystem ${SYSROOT}/usr/include

#we are going to install the modules under $PREFIX/mod/
PREFIX = 

SRC = $(shell find -name '*.c')
MOD = ${SRC:.c=.ko}

all : ${MOD}

%.ko : %.c
	${CC} -D${ARCH} ${CFLAGS} -o $@ -c $^
install :
	mkdir -p ${PREFIX}/mod
	cp *.ko ${PREFIX}/mod/
clean :
	rm -f *.ko