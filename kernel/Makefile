# automatically generated from tmakegen
# DO NOT EDIT

# tconf might have generated a config.mk
-include config.mk

# a few standard variables
VERSION ?= $(shell git describe --tags --always 2>/dev/null || echo unknown)
NAME ?= kernel
BUILDDIR ?= ../build
PREFIX ?= /usr/local
STATIC ?= yes
SHARED ?= no
CFLAGS ?= -Wall -Wextra
CFLAGS += -DVERSION='"$(VERSION)"'

ifeq ($(HAVE_MMD) $(HAVE_MP),yes yes)
	CFLAGS += -MMD -MP
endif

ifeq ($(V),1)
	Q =
else
	Q = @
endif

.PHONY : all
all :

.PHONY : install
install :

.PHONY : uninstall
uninstall :

KLIBC ?= ../tlibc/libk.a

# freestanding flags
CFLAGS += -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
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

# ==== kernel target ====

ALL_kernel = $(BUILDDIR)/kernel/kernel
SRC_kernel = drivers/device.c drivers/edid.c drivers/framebuffer.c drivers/input.c drivers/liminefb.c drivers/mem.c drivers/pty.c drivers/terminal_emu.c drivers/trm.c drivers/tty.c fs/cache.c fs/pipe.c fs/poll.c fs/procfs.c fs/socket.c fs/sysfs.c fs/tarfs.c fs/tmpfs.c fs/unix.c fs/vfs.c klib/ini.c klib/kout.c klib/list.c klib/print.c klib/ringbuf.c klib/string.c klib/time.c mem/kheap.c mem/pmm.c mem/slab.c mem/vmm.c task/cond.c task/exec.c task/fork.c task/futex.c task/interrupt.c task/mutex.c task/rwsem.c task/scheduler.c task/signal.c task/sleep.c task/syscall.c task/waitfor.c bootinfo.c main.c modules.c ubsan.c $(shell find arch/$(ARCH) -name "*.c" -or -name "*.s")
OBJ_kernel = $(SRC_kernel:%=$(BUILDDIR)/kernel/%.o)
DEPS_kernel = $(SRC_kernel:%=$(BUILDDIR)/kernel/%.d)

.PHONY : all-kernel
all : all-kernel
all-kernel : $(ALL_kernel)

# include dependencies files
-include $(DEPS_kernel)

.PHONY : install-kernel
install : install-kernel
install-kernel : all-kernel
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	@echo "INSTALL $(ALL_kernel)"
	$(Q)cp $(ALL_kernel) "$(DESTDIR)$(PREFIX)/bin"

.PHONY : uninstall-kernel
uninstall : uninstall-kernel
uninstall-kernel :
	@echo "UNINSTALL $(DESTDIR)$(PREFIX)/bin/kernel"
	$(Q)rm -f "$(DESTDIR)$(PREFIX)/bin/kernel"

.PHONY : clean-kernel
clean-kernel :
	@echo "CLEAN $(BUILDDIR)/kernel"
	$(Q)rm -fr "$(BUILDDIR)/kernel"

$(BUILDDIR)/kernel/%.c.o : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(BUILDDIR)/kernel/%.c.o : $(BUILDDIR)/kernel/%.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(BUILDDIR)/kernel/kernel : $(OBJ_kernel) $(KLIBC) $(BUILDDIR)/kernel/sym.c.o
	@mkdir -p "$(@D)"
	@echo "CCLD kernel"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# sym.c is an automatically generated table
$(BUILDDIR)/kernel/sym.c : $(OBJ_kernel) $(KLIBC)
	@echo "GEN sym.c"
	@echo '#include <kernel/sym.h>' > $@
	@$(foreach SYM , $(shell $(NM) $^ --defined-only -ngj --quiet ),  echo "extern void $(SYM) ();" >> $@; )
	@echo 'const sym_t symbols[] = {' >> $@
	@$(foreach SYM , $(shell $(NM) $^ --defined-only -ngj --quiet ),  echo "	{ .name = \"$(SYM)\" , .value = (uintptr_t)$(SYM) , .size = sizeof($(SYM))}," >> $@; ) 
	@echo '};' >> $@
	@echo 'const size_t symbols_count = sizeof(symbols) / sizeof(sym_t);' >> $@

# ==== headers target ====
SRC_headers = include/arch/arch.h include/arch/irq.h include/arch/mmu.h include/arch/x86_64/asm.h include/arch/x86_64/cmos.h include/arch/x86_64/gdt.h include/arch/x86_64/idt.h include/arch/x86_64/isr.h include/arch/x86_64/paging.h include/arch/x86_64/pic.h include/arch/x86_64/pit.h include/arch/x86_64/port.h include/arch/x86_64/serial.h include/arch/x86_64/tss.h include/arch/x86_64/x86_64.h include/assert.h include/bootinfo.h include/drivers/bus.h include/drivers/device.h include/drivers/edid.h include/drivers/framebuffer.h include/drivers/input.h include/drivers/terminal_emu.h include/drivers/trm.h include/drivers/tty.h include/fs/cache.h include/fs/pipe.h include/fs/poll.h include/fs/procfs.h include/fs/socket.h include/fs/sysfs.h include/fs/tarfs.h include/fs/tmpfs.h include/fs/unix.h include/fs/vfs.h include/kernel.h include/klib/hashmap.h include/klib/ini.h include/klib/kout.h include/klib/list.h include/klib/print.h include/klib/ringbuf.h include/klib/string.h include/klib/time.h include/klib/vector.h include/macro.h include/mem/kheap.h include/mem/page.h include/mem/pmm.h include/mem/slab.h include/mem/vmm.h include/module.h include/panic.h include/sym.h include/task/async.h include/task/cond.h include/task/exec.h include/task/fork.h include/task/futex.h include/task/interrupt.h include/task/mutex.h include/task/rwlock.h include/task/rwsem.h include/task/scheduler.h include/task/signal.h include/task/sleep.h include/task/spinlock.h include/task/sys.h include/task/userspace.h ../limine/limine.h
FILES_headers = arch.h irq.h mmu.h asm.h cmos.h gdt.h idt.h isr.h paging.h pic.h pit.h port.h serial.h tss.h x86_64.h assert.h bootinfo.h bus.h device.h edid.h framebuffer.h input.h terminal_emu.h trm.h tty.h cache.h pipe.h poll.h procfs.h socket.h sysfs.h tarfs.h tmpfs.h unix.h vfs.h kernel.h hashmap.h ini.h kout.h list.h print.h ringbuf.h string.h time.h vector.h macro.h kheap.h page.h pmm.h slab.h vmm.h module.h panic.h sym.h async.h cond.h exec.h fork.h futex.h interrupt.h mutex.h rwlock.h rwsem.h scheduler.h signal.h sleep.h spinlock.h sys.h userspace.h limine.h
DEST_headers = $(FILES_headers:%=$(DESTDIR)$(PREFIX)/include/kernel/%)

.PHONY : install-headers
install : install-headers
install-headers :
	@mkdir -p "$(DESTDIR)$(PREFIX)/include/kernel"
	@echo "INSTALL_DATA $(SRC_headers)"
	$(Q)cp -r $(SRC_headers) "$(DESTDIR)$(PREFIX)/include/kernel"
.PHONY : uninstall-headers
uninstall : uninstall-headers
uninstall-headers :
	@echo "UNINSTALL $(DEST_headers)"
	$(Q)rm -fr $(DEST_headers)

.PHONY : targets
targets :
	@echo "====== kernel targets ======"
	@echo "====== globals targets ======"
	@echo "all       : build every component"
	@echo "install   : install every component"
	@echo "uninstall : uninstall every component"
	@echo "clean     : clean every component"
	@echo "====== kernel targets ======"
	@echo "all-kernel       : build kernel"
	@echo "install-kernel   : install kernel"
	@echo "uninstall-kernel : uninstall kernel"
	@echo "clean-kernel     : clean kernel"

Makefile : tmakegen ../scripts/tmake.sh
	@echo "GEN Makefile"
	$(Q)./tmakegen

.PHONY : clean
clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q)rm -fr "$(BUILDDIR)"
