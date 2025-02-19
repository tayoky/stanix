#output
hdd_out = stanix.hdd
iso_out = stanix.iso
OUT = out
KERNEL = stanix.elf
MAKEFLAGS += --no-builtin-rules
SYSROOT = sysroot

#tools
export CC = gcc
export LD = ld 
export NASM = nasm

out_files = ${OUT}/boot/limine/limine-bios.sys \
${OUT}/EFI/BOOT/BOOTX64.EFI \
${OUT}/EFI/BOOT/BOOTIA32.EFI \
${OUT}/boot/limine/limine-bios-cd.bin\
${OUT}/boot/limine/limine-uefi-cd.bin\
${OUT}/boot/initrd.tar

kernel_src = $(shell find ./kernel -name "*")
initrd_src = $(shell find ./initrd -name "*")

all : hdd iso

test : hdd
	qemu-system-x86_64 -drive file=${hdd_out}  -serial stdio
	
hdd : build ${hdd_out}
${hdd_out} : ${kernel_src} ${out_files} 
	rm -f ${hdd_out}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${hdd_out}
	sgdisk ${hdd_out} -n 1:2048 -t 1:ef00 
	make -C limine
# Install the Limine BIOS stages onto the image.
	./limine/limine bios-install ${hdd_out}
# Format the image as fat32.
	mformat -i ${hdd_out}@@1M	
#copy the files
	cd ${OUT} && mcopy -i ../${hdd_out}@@1M * -/ ::/

iso : build ${iso_out}
${iso_out} : ${kernel_src} ${out_files}
	rm -f ${iso_out}
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ${OUT} -o ${iso_out}

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

#build!!!
build : header
	${MAKE} -C kernel OUT=../${OUT} KERNEL=${KERNEL} SYSROOT=../${SYSROOT}
	${MAKE} -C tlibc install TARGET=stanix SYSROOT=../${SYSROOT}
	${MAKE} -C userspace install SYSROOT=../${SYSROOT}
header : 
	${MAKE} -C tlibc header TARGET=stanix SYSROOT=../${SYSROOT}
clean :
	cd kernel && make clean