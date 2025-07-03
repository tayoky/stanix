MAKEFLAGS += --no-builtin-rules
PREFIX = ${SYSROOT}/usr
export PREFIX

#first get all the sub directory
libs = $(shell find -name "*" -not -path "." -type d -maxdepth 1)

all :
	@$(foreach dir,${libs},${MAKE} -C ${dir} && ) true
install : all
	@$(foreach dir,${libs},${MAKE} -C ${dir} install && ) true
clean :
	@$(foreach dir,${libs},${MAKE} -C ${dir} clean && ) true