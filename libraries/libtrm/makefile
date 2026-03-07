MAKEFLAGS += --no-builtin-rules

SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#cc flags
CFLAGS += -Wall -Wextra -std=c99 -fpic
CFLAGS += -Iinclude

all : libtrm.so

libtrm.so : ${OBJ}
	${CC} ${CFLAGS} -shared -o $@ $^

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^
	
install : all
	@echo '[install headers]'
	@mkdir -p ${PREFIX}/include/
	@cp -r include/* ${PREFIX}/include/
	@echo '[install libtrm.so]'
	@mkdir -p ${PREFIX}/lib
	@cp libtrm.so ${PREFIX}/lib

clean : 
	rm -f ${OBJ}
