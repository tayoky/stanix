# makefile include to provide x86_64 kflags

KFLAGS += \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-march=x86-64 \
	-m64 \
	-mno-red-zone \
	-mcmodel=large
