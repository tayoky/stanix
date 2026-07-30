TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

PROG = stanix.elf
KLIBC ?= $(SYSROOT)/usr/lib/libk.a

# we need to install in /boot
BINDIR = /boot

# freestanding flags
CFLAGS += \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-PIC \
	-fno-omit-frame-pointer \
	-fsanitize=undefined \
	-D__KERNEL__=1
CFLAGS += -I ./include/
CFLAGS += -I ../limine/

LDFLAGS += \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	-T arch/$(ARCH)/linker.ld

# arch specific flags
-include arch/$(ARCH)/arch.mk

INCS = $(wildcard include/*.h include/*/*.h include/arch/$(ARCH)/*.h) ../limine/limine.h
INCSDIR := $(INCSDIR)/kernel
DIRS = drivers fs klib mem task
SRCS = $(wildcard *.c) $(foreach DIR,$(DIRS), $(wildcard $(DIR)/*.c $(DIR)/*/*.c)) sym.c
include $(TMAKE_DIR)/tmake-prog.mk

# kernel need to link with klibc
# we use this instead of -lk so it rebuild if klibc is rebuilt
$(BUILDDIR)/$(PROG) : $(KLIBC)

# sym.c is an automatically generated table
$(BUILDDIR)/sym.c : $(filter-out $(BUILDDIR)/sym.o, $(OBJS)) $(KLIBC)
	@echo "GEN sym.c"
	@echo '#include <kernel/sym.h>' > $@
	@$(foreach SYM , $(shell $(NM) $^ --defined-only -ngj --quiet ),  echo "extern void $(SYM) ();" >> $@; )
	@echo 'const sym_t symbols[] = {' >> $@
	@$(foreach SYM , $(shell $(NM) $^ --defined-only -ngj --quiet ),  echo "	{ .name = \"$(SYM)\" , .value = (uintptr_t)$(SYM) , .size = sizeof($(SYM))}," >> $@; ) 
	@echo '};' >> $@
	@echo 'const size_t symbols_count = sizeof(symbols) / sizeof(sym_t);' >> $@
