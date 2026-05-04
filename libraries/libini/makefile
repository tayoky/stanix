MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = $(SRC:.c=.o)

#cc flags
CFLAGS += -Wall -Wextra -std=c99 -fpic
CFLAGS += -Iinclude

all : libini.so

libini.so : $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/include"
	@cp include/*.h "$(DESTDIR)$(PREFIX)/include"
	@echo '[install libini.so]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	@cp libini.so "$(DESTDIR)$(PREFIX)/lib"

clean : 
	rm -f $(OBJ)