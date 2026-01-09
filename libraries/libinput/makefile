MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -Iinclude

all : libinput.a

libinput.a : ${OBJ}
	${AR} rcs $@ $^

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p ${PREFIX}/include
	@cp include/*.h ${PREFIX}/include
	@echo '[install libinput.a]'
	@mkdir -p ${PREFIX}/lib
	@cp libinput.a ${PREFIX}/lib

clean : 
	rm -f ${OBJ}