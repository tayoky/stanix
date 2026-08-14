# makefile include to provide kflags

KFLAGS = \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -fsanitize=undefined \
	-mno-red-zone \

-include $(TMAKE_DIR)/stanix-$(ARCH)-kflags.mk
