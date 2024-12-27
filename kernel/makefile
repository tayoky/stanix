#output
include out.mk

#define your compilator and other tools here
CC = gcc
LD = ld

#flag
LDFLAGS = 

#all sources
src_c = $(shell find *.c)
OBJ = ${src_c:.c=.o}

#DO NOT EDIT
#except if you know what you are doing
LDFLAGS += \
    -Wl,-m,elf_x86_64 \
    -Wl,--build-id=none \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T linker.ld
include include.mk

${out}/boot/${kernel} : ${OBJ}
	${LD} ${LDFLAGS} ${OBJ} ${out}/boot/${kernel}

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
clean :
	rm ${OBJ}