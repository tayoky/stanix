MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -Iinclude

all :

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p ${PREFIX}/include/libutils
	@cp include/libutils/*.h ${PREFIX}/include/libutils

clean : 
	rm -f ${OBJ}