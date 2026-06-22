MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = $(SRC:.c=.o)

#cc flags
CFLAGS += -Wall -Wextra -std=c99 -fpic
CFLAGS += -Iinclude -I./

all : libtwm.so

libtwm.so : $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/include"
	@cp include/*.h "$(DESTDIR)$(PREFIX)/include"
	@echo '[install libtwm.so]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	@cp libtwm.so "$(DESTDIR)$(PREFIX)/lib"

clean : 
	rm -f $(OBJ)
