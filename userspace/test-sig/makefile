
#first get all the src
SRC = $(shell find -name "*.c")
OBJ = ${SRC:.c=.o}

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename ${PWD})

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -I ./

all : ../bin/${OUT}

../bin/${OUT} : ${OBJ}
	${CC} ${LDFLAGS} -o $@ $^

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^

clean : 
	rm -f ${OBJ}