#output
hdd_out = stanix.hdd
iso_out = stanix.iso
OUT = out
KERNEL = stanix.elf
MAKEFLAGS += --no-builtin-rules

export TOP = ${PWD}

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

out_files = ${OUT}/boot/limine/limine-bios.sys \
${OUT}/EFI/BOOT/BOOTX64.EFI \
${OUT}/EFI/BOOT/BOOTIA32.EFI \
${OUT}/EFI/BOOT/BOOTAA64.EFI \
${OUT}/boot/limine/limine-bios-cd.bin\
${OUT}/boot/limine/limine-uefi-cd.bin\
${OUT}/boot/initrd.tar

kernel_src = $(shell find ./kernel -name "*")
initrd_src = $(shell find ./initrd -name "*")

all : hdd iso

test : hdd
	qemu-system-${ARCH} \
	-drive file=${hdd_out},if=none,id=nvm -serial stdio \
	-device nvme,serial=deadbeef,drive=nvm
ata-test : hdd
	qemu-system-${ARCH} \
	-hda ${hdd_out} -serial stdio
cdrom-test : iso
	qemu-system-${ARCH} -cdrom stanix.iso -serial stdio  --no-shutdown --no-reboot
debug : hdd
	objdump -D ${OUT}/boot/${KERNEL} > asm.txt
	qemu-system-${ARCH} -drive file=${hdd_out}  -serial stdio -s -S
hdd : build ${hdd_out}
${hdd_out} : ${kernel_src} ${out_files} 
	@echo "[creating hdd image]"
	@rm -f ${hdd_out}
	@dd if=/dev/zero bs=5M count=0 seek=64 of=${hdd_out}
	sgdisk ${hdd_out} -n 1:2048 -t 1:ef00 
	@make -C limine
# Format the image as fat32.
	@echo "[format fat32 partition]"
	@mformat -i ${hdd_out}@@1M	
#copy the files
	@echo "[copying boot files]"
	@cd ${OUT} && mcopy -i ../${hdd_out}@@1M * -/ ::/
# Install the Limine BIOS stages onto the image.
	@echo "[installing limine]"
	@./limine/limine bios-install ${hdd_out}

iso : build ${iso_out}
${iso_out} : ${kernel_src} ${out_files}
	@echo "[creating iso]"
	@rm -f ${iso_out}
	@xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ${OUT} -o ${iso_out}
	@make -C limine
	@echo "[installing limine]"
	@./limine/limine bios-install ${iso_out}
OVMF-img.bin : OVMF.fd
	@cp OVMF.fd OVMF-img.bin
	dd if=/dev/zero of=OVMF-img.bin bs=1 count=0 seek=67108864

#limine files to copy
${OUT}/EFI/BOOT/% : limine/%
	@echo "[installing EFI/BOOT/$(shell basename $@)]"
	@mkdir -p ${OUT}/EFI/BOOT/
	@cp  $^ $@
${OUT}/boot/limine/limine-% : limine/limine-%
	@echo "[installing boot/$^]"
	@mkdir -p ${OUT}/boot/limine/
	@cp  $^ $@

#for build the ramdisk
rd : ${OUT}/boot/initrd.tar
${OUT}/boot/initrd.tar : ${initrd_src}
	@echo "[creating init ramdisk]"
	@mkdir -p ${OUT}/boot
	@mkdir -p initrd/dev initrd/tmp
	@chmod +s  initrd/bin/login initrd/bin/sudo
	@cd initrd && tar --create -f ../${OUT}/boot/initrd.tar **

${OUT}/boot/limine/limine.conf : limine.conf
	@echo "[installing boot/limine/$^]"
	@mkdir -p ${OUT}/boot/limine/
	@cp $^ $@
#build!!!
build : header ${OUT}/boot/limine/limine.conf
	@mkdir -p ${OUT}/boot/
	@${MAKE} -C kernel OUT=../${OUT} KERNEL=${KERNEL} SYSROOT=${SYSROOT}
	@${MAKE} -C tlibc install TARGET=stanix
	@${MAKE} -C libraries install
	@${MAKE} -C userspace install SYSROOT=${SYSROOT}
	@${MAKE} -C modules
	@${MAKE} -C modules install PREFIX=$(shell realpath ./initrd)
header : 
	@${MAKE} -C kernel header SYSROOT=${SYSROOT}
	@${MAKE} -C modules header
	@${MAKE} -C tlibc header TARGET=stanix
	@echo "[installing limine.h]"
	@cp ./limine/limine.h ${SYSROOT}/usr/include/kernel
clean :
	@${MAKE} -C kernel clean
	@${MAKE} -C tlibc clean
	@${MAKE} -C userspace clean
	@${MAKE} -C modules clean
config.mk :
	$(error "run ./configure before runing make")
