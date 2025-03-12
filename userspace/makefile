MAKEFLAGS += --no-builtin-rules
OUT = 
SYSROOT = 

#first get all the sub directory
apps = $(shell find -name "*" -not -path "./out" -not -path "." -not -path "./lib" -type d -exec basename \{} \;)

all :
	mkdir -p out
	$(foreach dir,${apps},${MAKE} -C ${dir} SYSROOT=${SYSROOT} &&) true
	$(foreach dir,${apps},mv ${dir}/${dir} out && )true

#for installing to the initrd
install : all
	mkdir -p ../initrd/bin
	cd out && cp -r ./ ../../initrd/bin
clean :
	$(foreach dir,${apps},${MAKE} -C ${dir} clean SYSROOT=${SYSROOT})