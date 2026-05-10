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

# ==== kernel target ====

ALL_kernel = $(BUILDDIR)/kernel/kernel
SRC_kernel = drivers/input.c drivers/edid.c drivers/terminal_emu.c drivers/trm.c drivers/device.c drivers/pty.c drivers/mem.c drivers/tty.c drivers/liminefb.c drivers/framebuffer.c fs/socket.c fs/procfs.c fs/sysfs.c fs/tmpfs.c fs/pipe.c fs/cache.c fs/unix.c fs/tarfs.c fs/vfs.c fs/poll.c klib/print.c klib/string.c klib/ini.c klib/time.c klib/kout.c klib/ringbuf.c klib/list.c mem/pmm.c mem/slab.c mem/kheap.c mem/vmm.c task/futex.c task/exec.c task/signal.c task/cond.c task/fork.c task/waitfor.c task/mutex.c task/sleep.c task/interrupt.c task/rwsem.c task/syscall.c task/scheduler.c bootinfo.c main.c modules.c ubsan.c
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

$(BUILDDIR)/kernel/kernel : $(OBJ_kernel)
	@mkdir -p "$(@D)"
	@echo "CCLD kernel"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# ==== headers target ====
SRC_headers = include/mem/vmm.h include/mem/page.h include/mem/pmm.h include/mem/slab.h include/mem/kheap.h include/drivers/bus.h include/drivers/trm.h include/drivers/edid.h include/drivers/device.h include/drivers/input.h include/drivers/tty.h include/drivers/terminal_emu.h include/drivers/framebuffer.h include/task/exec.h include/task/fork.h include/task/spinlock.h include/task/rwlock.h include/task/async.h include/task/userspace.h include/task/signal.h include/task/sys.h include/task/cond.h include/task/sleep.h include/task/futex.h include/task/rwsem.h include/task/interrupt.h include/task/mutex.h include/task/scheduler.h include/kernel.h include/macro.h include/module.h include/panic.h include/arch/mmu.h include/arch/arch.h include/arch/x86_64/port.h include/arch/x86_64/asm.h include/arch/x86_64/tss.h include/arch/x86_64/idt.h include/arch/x86_64/serial.h include/arch/x86_64/cmos.h include/arch/x86_64/gdt.h include/arch/x86_64/isr.h include/arch/x86_64/pic.h include/arch/x86_64/pit.h include/arch/x86_64/paging.h include/arch/x86_64/x86_64.h include/arch/irq.h include/fs/vfs.h include/fs/socket.h include/fs/tarfs.h include/fs/pipe.h include/fs/procfs.h include/fs/poll.h include/fs/cache.h include/fs/unix.h include/fs/tmpfs.h include/fs/sysfs.h include/sym.h include/assert.h include/klib/ringbuf.h include/klib/vector.h include/klib/string.h include/klib/ini.h include/klib/print.h include/klib/kout.h include/klib/list.h include/klib/hashmap.h include/klib/time.h include/bootinfo.h ../limine/limine.h
FILES_headers = vmm.h page.h pmm.h slab.h kheap.h bus.h trm.h edid.h device.h input.h tty.h terminal_emu.h framebuffer.h exec.h fork.h spinlock.h rwlock.h async.h userspace.h signal.h sys.h cond.h sleep.h futex.h rwsem.h interrupt.h mutex.h scheduler.h kernel.h macro.h module.h panic.h mmu.h arch.h port.h asm.h tss.h idt.h serial.h cmos.h gdt.h isr.h pic.h pit.h paging.h x86_64.h irq.h vfs.h socket.h tarfs.h pipe.h procfs.h poll.h cache.h unix.h tmpfs.h sysfs.h sym.h assert.h ringbuf.h vector.h string.h ini.h print.h kout.h list.h hashmap.h time.h bootinfo.h limine.h
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

Makefile :  /home/tayoky/git-repo/stanix/scripts/tmake.sh
	@echo "GEN Makefile"
	$(Q)./

.PHONY : clean
clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q)rm -fr "$(BUILDDIR)"
