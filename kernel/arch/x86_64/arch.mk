CFLAGS += \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-march=x86-64 \
	-m64 \
	-mno-red-zone \
	-mcmodel=kernel
LDFLAGS += -Wl,-m,elf_x86_64

# we need to use NASM
AS = $(NASM)
ASFLAGS = -f elf64

