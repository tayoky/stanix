CFLAGS += \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-march=x86-64 \
	-m64 \
	-mno-red-zone \
	-mcmodel=kernel
LDFLAGS += -m elf_x86_64

ASMFLAGS += -f elf64

$(BUILDDIR)/%.o : %.s
	@echo "NASM $<"
	$(Q)$(NASM) $(ASMFLAGS) $< -o $@