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

ASMFLAGS += -f elf64

$(BUILDDIR)/kernel/%.s.o : %.s
	@mkdir -p "$(@D)"
	@echo "NASM $<"
	$(Q)$(NASM) $(ASMFLAGS) $< -o $@
