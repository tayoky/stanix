CFLAGS += \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
	-march=i386 \
	-m32 \
	-mno-red-zone
LDFLAGS += -m elf_i386

ASMFLAGS += -f elf32

%.o : %.s
	${NASM} ${ASMFLAGS} $< -o $@