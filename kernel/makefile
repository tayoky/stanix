#output
OUT = out
KERNEL = stanix.elf

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

${OUT}/boot/${KERNEL} : ${OBJ}
	${LD} ${LDFLAGS} ${OBJ} -o ${OUT}/boot/${KERNEL}

${OUT}/boot/limine/limine.conf : limine.conf
	cp limine.conf ${out}/boot/limine/limine.conf
%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
%.o : %.s
	${NASM} ${ASMFLAGS} $< -o $@
clean :
	rm ${OBJ} ${out}