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
PREFIX ?= /usr

SRC = $(shell find -name '*.c')
MOD = $(SRC:.c=.ko)

all : $(MOD) header

%.ko : %.c
	$(CC) -D$(ARCH) $(CFLAGS) -o $@ -c $^ -DMODULE

install : all
	mkdir -p "$(DESTDIR)$(PREFIX)/mod"
	cp $(MOD) "$(DESTDIR)$(PREFIX)/mod"

header :
	mkdir -p "$(DESTDIR)$(PREFIX)/include/module"
	cp include/*.h "$(DESTDIR)$(PREFIX)/include/module"

clean :
	rm -f $(MOD)
