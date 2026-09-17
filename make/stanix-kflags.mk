# makefile include to provide kflags

ARCH ?= $(word 1,$(subst -, ,$(HOST)))

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
