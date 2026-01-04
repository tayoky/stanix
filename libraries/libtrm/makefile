MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -Iinclude

all : libtrm.a

libtrm.a : ${OBJ}
	${AR} rcs $@ $^

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p ${PREFIX}/include/
	@cp -r include/* ${PREFIX}/include/
	@echo '[install libtrm.a]'
	@mkdir -p ${PREFIX}/lib
	@cp libtrm.a ${PREFIX}/lib

clean : 
	rm -f ${OBJ}
