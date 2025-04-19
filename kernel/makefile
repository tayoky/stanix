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
src_h = $(shell find -name "*.h" -not -path "./arch/*") $(shell find ./arch/${ARCH} -name "*.h")
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

${OUT}/boot/${KERNEL} : ${OBJ} sym.o
	${LD} ${LDFLAGS} ${OBJ} sym.o -o ${OUT}/boot/${KERNEL}
%.o : %.c
	${CC} -D${ARCH} ${CFLAGS} -o $@ -c $^
sym.o : ${OBJ}
	@echo '#include <kernel/sym.h>' > sym.c
	@$(foreach SYM , $(shell nm ${OBJ} --defined-only -ngj --quiet ),  echo "extern void ${SYM} ();" >> sym.c ; )
	@echo 'const sym_t symbols[] = {' >> sym.c
	@$(foreach SYM , $(shell nm ${OBJ} --defined-only -ngj --quiet ),  echo "	{ .name = \"${SYM}\" , .value = (uintptr_t)${SYM} , .size = sizeof(${SYM})}," >> sym.c ; ) 
	@echo '};' >> sym.c
	@echo 'const size_t symbols_count = sizeof(symbols) / sizeof(sym_t);' >> sym.c
	${CC} ${CFLAGS} -o sym.o -c sym.c 
	@rm sym.c
clean :
	rm -f ${OBJ} ${out}
header :
	mkdir -p ${SYSROOT}/usr/include/kernel
	cp ${src_h} \
	${SYSROOT}/usr/include/kernel