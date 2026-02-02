#output
HDD_IMAGE = stanix.hdd
ISO_IMAGE = stanix.iso
OUT = out
KERNEL = stanix.elf
MAKEFLAGS += --no-builtin-rules

export TOP = $(PWD)

include config.mk

#tools
export CC
export LD
export AS
export AR
export NM
export NASM
export ARCH
export SYSROOT
export CFLAGS
export LDFLAGS

OUT_FILES = $(OUT)/boot/limine/limine-bios.sys \
$(OUT)/EFI/BOOT/BOOTX64.EFI \
$(OUT)/EFI/BOOT/BOOTIA32.EFI \
$(OUT)/EFI/BOOT/BOOTAA64.EFI \
$(OUT)/boot/limine/limine-bios-cd.bin \
$(OUT)/boot/limine/limine-uefi-cd.bin \
$(OUT)/boot/initrd.tar \
$(OUT)/boot/limine/limine.conf

INITRD_SRC = $(shell find ./initrd)

all : hdd iso

test : hdd
	qemu-system-$(ARCH) \
	-drive file=$(HDD_IMAGE),if=none,id=nvm -serial stdio \
	-device nvme,serial=deadbeef,drive=nvm
ata-test : hdd
	qemu-system-$(ARCH) \
	-hda $(HDD_IMAGE) -serial stdio 
#--trace "ide_*"

cdrom-test : iso
	qemu-system-$(ARCH) -cdrom stanix.iso -serial stdio  --no-shutdown --no-reboot

debug : hdd
	objdump -D $(OUT)/boot/$(KERNEL) > asm.txt
	qemu-system-$(ARCH) -drive file=$(HDD_IMAGE)  -serial stdio -s -S

hdd : build $(HDD_IMAGE)
$(HDD_IMAGE) : build $(OUT_FILES) 
	@echo "[creating hdd image]"
	@rm -f $(HDD_IMAGE)
	@dd if=/dev/zero bs=5M count=0 seek=64 of=$(HDD_IMAGE)
	sgdisk $(HDD_IMAGE) -n 1:2048 -t 1:ef00 
	@make -C limine
# Format the image as fat32.
	@echo "[format fat32 partition]"
	@mformat -i $(HDD_IMAGE)@@1M	
#copy the files
	@echo "[copying boot files]"
	@cd $(OUT) && mcopy -i ../$(HDD_IMAGE)@@1M * -/ ::/
# Install the Limine BIOS stages onto the image.
	@echo "[installing limine]"
	@./limine/limine bios-install $(HDD_IMAGE)

iso : build $(ISO_IMAGE)
$(ISO_IMAGE) : $(kernel_src) $(out_files)
	@echo "[creating iso]"
	@rm -f $(ISO_IMAGE)
	@xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $(OUT) -o $(ISO_IMAGE)
	@make -C limine
	@echo "[installing limine]"
	@./limine/limine bios-install $(ISO_IMAGE)

OVMF-img.bin : OVMF.fd
	@cp OVMF.fd OVMF-img.bin
	dd if=/dev/zero of=OVMF-img.bin bs=1 count=0 seek=67108864

#limine files to copy
$(OUT)/EFI/BOOT/% : limine/%
	@echo "[installing EFI/BOOT/$(shell basename $@)]"
	@mkdir -p $(OUT)/EFI/BOOT/
	@cp  $^ $@
$(OUT)/boot/limine/limine-% : limine/limine-%
	@echo "[installing boot/$^]"
	@mkdir -p $(OUT)/boot/limine/
	@cp  $^ $@

#for build the ramdisk
rd : $(OUT)/boot/initrd.tar
$(OUT)/boot/initrd.tar : $(INITRD_SRC)
	@echo "[creating init ramdisk]"
	@mkdir -p $(OUT)/boot
	@mkdir -p initrd/dev initrd/tmp initrd/mnt initrd/proc initrd/sys
	@chmod +s  initrd/bin/login initrd/bin/sudo
#temporary until real sysroot, copy sysroot to initrd
	@cp -r $(SYSROOT)/* initrd/
	@cd initrd && tar --create -f ../$(OUT)/boot/initrd.tar **

$(OUT)/boot/limine/limine.conf : limine.conf
	@echo "[installing boot/limine/$^]"
	@mkdir -p $(OUT)/boot/limine/
	@cp $^ $@

# target to build subfolders
_tlibc : header
	@$(MAKE) -C tlibc install TARGET=stanix
_kernel : _tlibc header
	@$(MAKE) -C kernel PREFIX=$(shell realpath "$(OUT)") KERNEL=$(KERNEL) SYSROOT=$(SYSROOT)
_modules : _tlibc header
	@$(MAKE) -C modules install PREFIX=$(shell realpath ./initrd)
_libraries : _tlibc
	@$(MAKE) -C libraries install
_userspace : _tlibc _libraries
	@$(MAKE) -C userspace install SYSROOT=$(SYSROOT)
_tash : _tlibc
	@cd ports && ./build.sh tash
	@cd ports && ./install.sh tash
_tutils : _tlibc
	@cd ports && ./build.sh tutils
	@cd ports && ./install.sh tutils

#build!!!
build : header _tlibc _kernel _modules _libraries _userspace _tash _tutils $(OUT_FILES)

header : 
	@$(MAKE) -C kernel header SYSROOT=$(SYSROOT)
	@$(MAKE) -C modules header
	@$(MAKE) -C tlibc header TARGET=stanix
	@echo "[installing limine.h]"
	@cp ./limine/limine.h $(SYSROOT)/usr/include/kernel

clean :
	@$(MAKE) -C kernel clean
	@$(MAKE) -C tlibc clean
	@$(MAKE) -C userspace clean
	@$(MAKE) -C modules clean
	@cd ports && ./clean.sh tutils
	@cd ports && ./clean.sh tash
	rm -fr $(OUT)

config.mk :
	$(error "run ./configure before runing make")
