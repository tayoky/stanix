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
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T linker.ld
include include.mk

${out}/boot/${kernel} : ${OBJ}
	${LD} ${LDFLAGS} ${OBJ} -o ${out}/boot/${kernel}

${out}/boot/limine/limine.conf : limine.conf
	cp limine.conf ${out}/boot/limine/limine.conf
%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
clean :
	rm ${OBJ}