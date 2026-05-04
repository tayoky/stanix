MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = $(SRC:.c=.o)

#cc flags
CFLAGS += -Wall -Wextra -std=c99 -fpic
CFLAGS += -Iinclude

all : libtrm.so

libtrm.so : $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/include"/
	@cp -r include/* "$(DESTDIR)$(PREFIX)/include/"
	@echo '[install libtrm.so]'
	@mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	@cp libtrm.so "$(DESTDIR)$(PREFIX)/lib"

clean : 
	rm -f $(OBJ)
