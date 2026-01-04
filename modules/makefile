CFLAGS = -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -fsanitize=undefined \
    -mcmodel=large 

#we are going to install the modules under $PREFIX/mod/
PREFIX = 

SRC = $(shell find -name '*.c')
MOD = $(SRC:.c=.ko)

all : $(MOD) header

%.ko : %.c
	$(CC) -D$(ARCH) $(CFLAGS) -o $@ -c $^ -DMODULE
install :
	mkdir -p $(PREFIX)/mod
	cp $(MOD) $(PREFIX)/mod/
header :
	mkdir -p $(SYSROOT)/usr/include/module
	cp include/*.h $(SYSROOT)/usr/include/module
clean :
	rm -f $(MOD)
