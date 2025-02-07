MAKEFLAGS += --no-builtin-rules

#first get all the sub directory
apps = $(shell find -name "*" -not -path "./out" -not -path "." -type d -exec basename \{} \;)

all :
	mkdir -p out
	$(foreach dir,${apps},make -C ${dir})
	$(foreach dir,${apps},mv ${dir}/${dir} out)

#for installing to the initrd
install : all
	mkdir -p ../initrd/bin
	cd out && cp -r ./ ../../initrd/bin
clean :
	$(foreach dir,${apps},make -C ${dir} clean)