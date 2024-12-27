#output
hdd_out = FOS25.hdd
iso_out = FOS25.hdd
out = out
kernel = stanix.elf

out_files = ${out}/boot/${kernel} ${out}/boot/limine/limine.conf

all : hdd iso

test : all
	qemu-system-amd64 -drive file=${hdd_out}
hdd : ${hdd_out}
${hdd_out} : ${out_files}
	rm ${hdd_out}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${hdd_out}
	sgdisk ${hdd_out} -n 1:2048 -t 1:ef00 
	make -C limine
# Install the Limine BIOS stages onto the image.
	./limine/limine bios-install ${hdd_out}
# Format the image as fat32.
	mformat -i ${hdd_out}@@1M
# Make relevant subdirectories.
	mmd -i ${hdd_out}@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
#copy the files
	mcopy -i ${hdd_out}@@1M ${out}/boot/${kernel} ::/boot
	mcopy -i ${hdd_out}@@1M ${out}/boot/limine/limine.conf limine/limine-bios.sys ::/boot/limine
	mcopy -i ${hdd_out}@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i ${hdd_out}@@1M limine/BOOTIA32.EFI ::/EFI/BOOT
iso : ${out_files}
${out}/boot/${kernel} : kernel/*
	cd kernel && make ../${out}/boot/limine/limine.conf \
	&& make ../${out}/boot/${kernel}
kernel/out.mk : makefile
	echo out = ../${out} > kernel/out.mk
	echo kernel = ${kernel} >> kernel/out.mk
clean :
	cd kernel && make clean