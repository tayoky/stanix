#output
out = out
kernel = stanix.elf

out_files = ${out}/boot/${kernel} ${out}/boot/limine/limine.conf

all : hdd

hdd : ${out_files}

${out}/boot/${kernel} : kernel/*
	cd kernel && make ../${out}/boot/limine/limine.conf
	cd kernel && make ../${out}/boot/${kernel}
kernel/out.mk : makefile
	echo out = ../${out} > kernel/out.mk
	echo kernel = ${kernel} >> kernel/out.mk
clean :
	cd kernel && make clean