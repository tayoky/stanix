#output
hdd_out = FOS25.hdd
iso_out = FOS25.iso
out = out
kernel = stanix.elf

out_files = ${out}/boot/${kernel} ${out}/boot/limine/limine.conf ${out}/boot/limine/limine-bios.sys \
${out}/EFI/BOOT/BOOTX64.EFI \
${out}/EFI/BOOT/BOOTIA32.EFI \
${out}/boot/limine/limine-bios-cd.bin\
${out}/boot/limine/limine-uefi-cd.bin

all : hdd iso

test : all
	qemu-system-x86_64 -drive file=${hdd_out}  -serial stdio
hdd : kernel-out ${hdd_out}
${hdd_out} : ${out_files}
	rm -f ${hdd_out}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${hdd_out}
	sgdisk ${hdd_out} -n 1:2048 -t 1:ef00 
	make -C limine
# Install the Limine BIOS stages onto the image.
	./limine/limine bios-install ${hdd_out}
# Format the image as fat32.
	mformat -i ${hdd_out}@@1M	
#copy the files
	cd ${out} && mcopy -i ../${hdd_out}@@1M * -/ ::/
iso : kernel-out ${out_files}
	rm -f ${iso_out}
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ${out} -o ${iso_out}
kernel-out : 
	cd kernel && make ../${out}/boot/limine/limine.conf \
	&& make ../${out}/boot/${kernel}
kernel/out.mk : makefile
	echo out = ../${out} > kernel/out.mk
	echo kernel = ${kernel} >> kernel/out.mk
#limine files to copy
${out}/boot/limine/limine-bios.sys : limine/limine-bios.sys
	cp limine/limine-bios.sys ${out}/boot/limine
${out}/EFI/BOOT/% : limine/%
	mkdir -p ${out}/EFI/BOOT/
	cp  $^ $@
${out}/boot/limine/limine-% : limine/limine-%
	cp  $^ $@
clean :
	cd kernel && make clean