#output
OUT = out
KERNEL = stanix.elf

SYSROOT =

#flag
LDFLAGS = 
ASMFLAGS =

#all sources
src_c = $(shell find -name "*.c" -not -path "./arch/*") $(shell find ./arch/${ARCH} -name "*.c")
src_asm = $(shell find -name "*.s" -not -path "./arch/*") $(shell find ./arch/${ARCH} -name "*.s")
OBJ = ${src_c:.c=.o} ${src_asm:.s=.o}

#DO NOT EDIT
#except if you know what you are doing
LDFLAGS += \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T arch/${ARCH}/linker.ld
ASMFLAGS += -Wall \
    -f elf64
include include.mk

include arch/${ARCH}/arch.mk

CFLAGS += --sysroot=${SYSROOT} -isystem ${SYSROOT}/usr/include

${OUT}/boot/${KERNEL} : ${OBJ}
	${LD} ${LDFLAGS} ${OBJ} -o ${OUT}/boot/${KERNEL}

${OUT}/boot/limine/limine.conf : limine.conf
	cp limine.conf ${out}/boot/limine/limine.conf
%.o : %.c
	${CC} -D${ARCH} ${CFLAGS} -o $@ -g -c $^
clean :
	rm ${OBJ} ${out}