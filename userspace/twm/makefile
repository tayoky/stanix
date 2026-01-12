
#first get all the src
SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename ${PWD})

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -I ./

LDFLAGS += -lgfx -linput

all : ../bin/${OUT}

../bin/${OUT} : ${OBJ}
	${CC} -o $@ $^ ${LDFLAGS}

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^

clean : 
	rm -f ${OBJ}