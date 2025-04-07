#output
hdd_out = stanix.hdd
iso_out = stanix.iso
OUT = out
KERNEL = stanix.elf
MAKEFLAGS += --no-builtin-rules

include config.mk

#tools
export CC
export LD
export AS
export NASM
export ARCH
export SYSROOT

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
	qemu-system-${ARCH} -drive file=${hdd_out}  -serial stdio
debug : hdd
	objdump -D ${OUT}/boot/${KERNEL} > asm.txt
	qemu-system-${ARCH} -drive file=${hdd_out}  -serial stdio -s -S
hdd : build ${hdd_out}
${hdd_out} : ${kernel_src} ${out_files} 
	rm -f ${hdd_out}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${hdd_out}
	sgdisk ${hdd_out} -n 1:2048 -t 1:ef00 
	make -C limine
# Format the image as fat32.
	mformat -i ${hdd_out}@@1M	
#copy the files
	cd ${OUT} && mcopy -i ../${hdd_out}@@1M * -/ ::/
# Install the Limine BIOS stages onto the image.
	./limine/limine bios-install ${hdd_out}

iso : build ${iso_out}
${iso_out} : ${kernel_src} ${out_files}
	rm -f ${iso_out}
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ${OUT} -o ${iso_out}
	./limine/limine bios-install ${iso_out}
OVMF-img.bin : OVMF.fd
	cp OVMF.fd OVMF-img.bin
	dd if=/dev/zero of=OVMF-img.bin bs=1 count=0 seek=67108864

#limine files to copy
${OUT}/boot/limine/limine-bios.sys : limine/limine-bios.sys
	cp limine/limine-bios.sys ${OUT}/boot/limine
${OUT}/EFI/BOOT/% : limine/%
	mkdir -p ${OUT}/EFI/BOOT/
	cp  $^ $@
${OUT}/boot/limine/limine-% : limine/limine-%
	cp  $^ $@

#for build the ramdisk
rd : ${OUT}/boot/initrd.tar
${OUT}/boot/initrd.tar : ${initrd_src}
	cd initrd && tar --create -f ../${OUT}/boot/initrd.tar **

${OUT}/boot/limine/limine.conf : kernel/limine.conf
	cp kernel/limine.conf ${OUT}/boot/limine/limine.conf
#build!!!
build : header ${OUT}/boot/limine/limine.conf
	${MAKE} -C kernel OUT=../${OUT} KERNEL=${KERNEL} SYSROOT=${SYSROOT}
	${MAKE} -C tlibc install TARGET=stanix
	${MAKE} -C userspace install SYSROOT=${SYSROOT}
	${MAKE} -C modules
	${MAKE} -C modules install PREFIX=$(shell realpath ./initrd)
header : 
	${MAKE} -C kernel header  SYSROOT=${SYSROOT}
	${MAKE} -C tlibc header TARGET=stanix
clean :
	${MAKE} -C kernel clean
	${MAKE} -C tlibc clean
	${MAKE} -C userspace clean
	${MAKE} -C modules clean
config.mk :
	$(error "run ./configure before runing make")
