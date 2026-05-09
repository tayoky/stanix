#output
PREFIX ?=
KERNEL ?= stanix.elf

SYSROOT ?=
KLIBC ?= ../tlibc/libk.a
BUILDDIR ?= build

#flag
LDFLAGS = 
ASMFLAGS =

#all sources
SRC_C = $(shell find -name "*.c" -not -path "./arch/*") $(shell find ./arch/$(ARCH) -name "*.c")
SRC_ASM = $(shell find -name "*.s" -not -path "./arch/*") $(shell find ./arch/$(ARCH) -name "*.s")
SRC_H = $(shell find -name "*.h" -not -path "./arch/*") $(shell find ./arch/$(ARCH) -name "*.h")
OBJ = $(SRC_C:%.c=$(BUILDDIR)/%.o) $(SRC_ASM:%.s=$(BUILDDIR)/%.o) $(KLIBC)

#DO NOT EDIT
#except if you know what you are doing
LDFLAGS += \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	-T arch/$(ARCH)/linker.ld
ASMFLAGS += -Wall

include include.mk

ifeq (,$(filter clean,$(MAKECMDGOALS)))
include arch/$(ARCH)/arch.mk
endif

$(PREFIX)/boot/$(KERNEL) : $(BUILDDIR)/sym.o $(OBJ)
	@mkdir -p $(shell dirname $@)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) -D$(ARCH) $(CFLAGS) -o $@ -g -c $^

$(BUILDDIR)/sym.o : $(OBJ)
	@echo '#include <kernel/sym.h>' > $(BUILDDIR)/sym.c
	@$(foreach SYM , $(shell $(NM) $(OBJ) --defined-only -ngj --quiet ),  echo "extern void $(SYM) ();" >> $(BUILDDIR)/sym.c ; )
	@echo 'const sym_t symbols[] = {' >> $(BUILDDIR)/sym.c
	@$(foreach SYM , $(shell $(NM) $(OBJ) --defined-only -ngj --quiet ),  echo "	{ .name = \"$(SYM)\" , .value = (uintptr_t)$(SYM) , .size = sizeof($(SYM))}," >> $(BUILDDIR)/sym.c ; ) 
	@echo '};' >> $(BUILDDIR)/sym.c
	@echo 'const size_t symbols_count = sizeof(symbols) / sizeof(sym_t);' >> $(BUILDDIR)/sym.c
	$(CC) $(CFLAGS) -o $(BUILDDIR)/sym.o -c $(BUILDDIR)/sym.c 
	
clean :
	rm -f $(OBJ)

header :
	mkdir -p $(SYSROOT)/usr/include/kernel
	cp $(SRC_H) \
	$(SYSROOT)/usr/include/kernel