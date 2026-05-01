MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = $(SRC:.c=.o)

#cc flags
CFLAGS += -Wall -Wextra -std=c99 -fpic
CFLAGS += -Iinclude

all : libinput.so

libinput.so : $(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p $(PREFIX)/include
	@cp include/*.h $(PREFIX)/include
	@echo '[install libinput.so]'
	@mkdir -p $(PREFIX)/lib
	@cp libinput.so $(PREFIX)/lib

clean : 
	rm -f $(OBJ)