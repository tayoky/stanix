MAKEFLAGS += --no-builtin-rules
PREFIX = ../initrd

ifeq (${TOP},)
CFLAGS += -O2 -g
LDFLAGS += -g
export CFLAGS
endif

#first get all the sub directory
apps = $(shell find -name "*" -not -name "bin" -not -path "." -type d -maxdepth 1 -exec basename \{} \;)

all :
	@mkdir -p bin
	@$(foreach dir,${apps},${MAKE} -C ${dir} SYSROOT=${SYSROOT} &&) true

#for installing to the initrd
install : all
	@mkdir -p ${PREFIX}/bin
	cp $(foreach app,${apps},bin/${app}) ${PREFIX}/bin
clean :
	@$(foreach dir,${apps},${MAKE} -C ${dir} clean SYSROOT=${SYSROOT} && ) true