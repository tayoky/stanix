#output
hdd_out = FOS25.hdd
iso_out = FOS25.hdd
out = out
kernel = stanix.elf

out_files = ${out}/boot/${kernel} ${out}/boot/limine/limine.conf ${out}/boot/limine/limine-bios.sys ${out}/EFI/BOOT/BOOTX64.EFI ${out}/EFI/BOOT/BOOTIA32.EFI

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
clean :
	cd kernel && make clean