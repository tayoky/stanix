TOP = $(CURDIR)
TMAKE_DIR = $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

# defaults
HDD_IMAGE = stanix.hdd
ISO_IMAGE = stanix.iso
BUILDENV_SHELL = $(SHELL)

INITRD = $(BUILDDIR)/initrd
BASE_INITRD = $(CURDIR)/base/initrd
BASE_SYSROOT = $(CURDIR)/base/sysroot
BASE_INITRD_SRC = $(shell find $(BASE_INITRD) -type f)

ifeq ($(findstring clean,$(MAKECMDGOALS))$(findstring header, $(MAKECMDGOALS)),)
include config.mk
else
-include config.mk
endif

#tools
export CC
export LD
export AS
export AR
export NM
export NASM
export ARCH
export PREFIX
export SYSROOT
export DESTDIR=$(SYSROOT)
export CFLAGS
export LDFLAGS
export PATH
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=$(SYSROOT)/usr/lib/pkgconfig
export PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR)
export PKG_CONFIG_SYSROOT_DIR=$(SYSROOT)

ifneq ($(wildcard toolchain/bin/.),)
# we have a cross toolchain
export PATH := $(realpath toolchain/bin):$(PATH)
endif

ESP_ROOT = $(BUILDDIR)/esp

ESP_FILES = $(ESP_ROOT)/boot/limine/limine-bios.sys \
$(ESP_ROOT)/EFI/BOOT/BOOTX64.EFI \
$(ESP_ROOT)/EFI/BOOT/BOOTIA32.EFI \
$(ESP_ROOT)/EFI/BOOT/BOOTAA64.EFI \
$(ESP_ROOT)/boot/limine/limine-bios-cd.bin \
$(ESP_ROOT)/boot/limine/limine-uefi-cd.bin \
$(ESP_ROOT)/boot/limine/limine.conf \
$(ESP_ROOT)/boot/initrd.tar \
$(ESP_ROOT)/boot/stanix.elf

all : build-all image-all

# help targets

targets : help
help :
	@echo "Stanix's makefile"
	@echo "======================== build targets ========================"
	@echo "build-all or build : build every component of Stanix"
	@echo "build-tlibc        : build tlibc"
	@echo "build-kernel       : build the core kernel (not the modules)"
	@echo "build-modules      : build the kernel modules"
	@echo "build-libraries    : build the userspace libraries"
	@echo "build-initrd       : build the initial ramdisk"
	@echo "build-env          : launch a build env setup for cross compiling"
	@echo "======================== image targets ========================"
	@echo "image-all          : build every image"
	@echo "image-hdd          : build the hdd image($(HDD_IMAGE))"
	@echo "image-iso          : build the iso image($(ISO_IMAGE))"
	@echo "======================== tests targets ========================"
	@echo "test-qemu          : test the hdd image in qemu"
	@echo "test-qemu-kvm      : test the hdd image in qemu with kvm"
	@echo "test-qemu-nvme     : test the hdd image in qemu with a nvme"
	@echo "test-qemu-kvm-nvme : test the hdd image in qemu with a nvme and kvm"
	@echo "test-qemu-ata      : test the hdd image in qemu with an ATA disk"
	@echo "test-qemu-cdrom    : test the hdd image in qemu with an ATAPI cdrom"
	@echo "==================== miscellaneous targets ===================="
	@echo "targets or help    : show this help"
	@echo "header             : install kernel and libc headers in sysroot"
	@echo "clean              : clean everything"

# test targets

test-qemu : test-qemu-nvme
test-qemu-kvm : test-qemu-kvm-nvme

test-qemu-nvme : image-hdd
	qemu-system-$(ARCH) \
	-drive file=$(HDD_IMAGE),format=raw,if=none,id=nvm -serial stdio \
	-device nvme,serial=deadbeef,drive=nvm -m 512

test-qemu-kvm-nvme : image-hdd
	qemu-system-$(ARCH) \
	-drive file=$(HDD_IMAGE),format=raw,if=none,id=nvm -serial stdio \
	-device nvme,serial=deadbeef,drive=nvm -m 1024 -cpu host -enable-kvm -smp 1

test-qemu-ata : image-hdd
# make a copy since qemu want one image for each disk 
	cp $(HDD_IMAGE) copy.hdd
	qemu-system-$(ARCH) \
	-drive file=$(HDD_IMAGE),format=raw,if=none,id=nvm \
	-device nvme,serial=deadbeef,drive=nvm,bootindex=1 \
	-drive file=copy.hdd,format=raw,if=ide -serial stdio  -m 512
#--trace "ide_*"

test-qemu-cdrom : image-iso
	qemu-system-$(ARCH) -cdrom stanix.iso -serial stdio -m 512 --no-shutdown --no-reboot

test-qemu-debug : image-hdd
	objdump -D $(ESP_ROOT)/boot/stanix.elf > kernel.dump
	qemu-system-$(ARCH) -drive file=$(HDD_IMAGE)  -serial stdio -s -S

# images target

image-hdd : $(HDD_IMAGE)
$(HDD_IMAGE) : $(ESP_FILES) build-all
	@echo "GEN $@"
	@rm -f $(HDD_IMAGE)
	$(Q)dd if=/dev/zero bs=9M count=0 seek=64 of=$(HDD_IMAGE)
	$(Q)sgdisk $(HDD_IMAGE) -n 1:2048 -t 1:ef00 
	@$(MAKE) -C limine
# Format the image as fat32.
	$(Q)mformat -i $(HDD_IMAGE)@@1M	
#copy the files
	$(Q)cd $(ESP_ROOT) && mcopy -i $(abspath $(HDD_IMAGE))@@1M * -/ ::/
# Install the Limine BIOS stages onto the image.
	$(Q)./limine/limine bios-install $(HDD_IMAGE)

image-iso : $(ISO_IMAGE)
$(ISO_IMAGE) : $(ESP_FILES) build-all
	@echo "GEN $@"
	@rm -f $(ISO_IMAGE)
	$(Q)xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $(ESP_ROOT) $(SYSROOT) -o $(ISO_IMAGE) -V "STANIX"
	@$(MAKE) -C limine
	$(Q)./limine/limine bios-install $(ISO_IMAGE)

image-all : image-iso image-hdd

OVMF-img.bin : OVMF.fd
	@cp OVMF.fd OVMF-img.bin
	dd if=/dev/zero of=OVMF-img.bin bs=1 count=0 seek=67108864

# limine files to copy
$(ESP_ROOT)/EFI/BOOT/% : limine/%
	@mkdir -p $(@D)
	@echo "INSTALL EFI/BOOT/$(shell basename $@)"
	@cp  $^ $@

$(ESP_ROOT)/boot/limine/limine-% : limine/limine-%
	@mkdir -p $(@D)
	@echo "INSTALL boot/$^"
	@cp  $^ $@


# build targets

build-tlibc : header
	@$(MAKE) -C tlibc install TARGET=stanix

$(ESP_ROOT)/boot/stanix.elf : build-kernel
build-kernel : build-tlibc build-libraries header
# we need to install the kernel into the EFI system partition
	@$(MAKE) -C kernel install-bin DESTDIR="$(ESP_ROOT)" BUILDDIR=$(BUILDDIR)/kernel

build-modules : build-tlibc build-libraries header
# we need to install modules in the initrd as they are required to load the sysroot
	@$(MAKE) -C modules install DESTDIR="$(INITRD)" BUILDDIR=$(BUILDDIR)/modules

build-libraries : build-tlibc
	@$(MAKE) -C libraries install BUILDDIR=$(BUILDDIR)/libraries

build-userspace : build-tlibc build-libraries
	@$(MAKE) -C userspace install BUILDDIR=$(BUILDDIR)/userspace

build-sysroot : build-userspace
	@mkdir -p $(SYSROOT)/dev $(SYSROOT)/tmp $(SYSROOT)/mnt $(SYSROOT)/proc $(SYSROOT)/sys
	@cp -Pf -p -r $(BASE_SYSROOT)/* $(SYSROOT)/

build-initrd : $(ESP_ROOT)/boot/initrd.tar
$(ESP_ROOT)/boot/initrd.tar : $(BASE_INITRD_SRC) build-userspace build-modules build-sysroot
	@mkdir -p $(@D)
	@echo "GEN boot/initrd.tar"
	@mkdir -p $(INITRD)/dev $(INITRD)/tmp $(INITRD)/mnt
	@cp -Pf -p -r $(BASE_INITRD)/* $(INITRD)/
# temporary until real sysroot, copy sysroot to initrd
	@cp -Pf -p -r $(SYSROOT)/* $(INITRD)/
	@cd $(INITRD) && tar -cf $(ESP_ROOT)/boot/initrd.tar *

$(ESP_ROOT)/boot/limine/limine.conf : limine.conf
	@echo "INSTALL boot/limine/$^"
	@mkdir -p $(@D)
	@cp $^ $@

build-all : header build-tlibc build-kernel build-modules build-libraries build-userspace build-initrd build-sysroot
build : build-all

build-env :
	@echo "[starting build environement]"
	@$(BUILDENV_SHELL)

header : 
	@$(MAKE) -C kernel install-incs
	@$(MAKE) -C modules install-incs
	@$(MAKE) -C tlibc install-include TARGET=stanix
	@echo "INSTALL limine.h"
	@cp ./limine/limine.h $(DESTDIR)$(PREFIX)/include/kernel/

clean :
	@$(MAKE) -C kernel clean
	@$(MAKE) -C tlibc clean
	@$(MAKE) -C userspace clean
	@$(MAKE) -C modules clean
	@$(MAKE) -C libraries clean
	rm -fr $(BUILDDIR)

.PHONY : all targets help clean header build-tlibc build-kernel build-modules build-libraries build-userspace build-initrd build-all build
