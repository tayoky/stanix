
#first get all the src
SRC_C = $(shell find -name "*.c")
SRC_ASM = $(shell find -name "*.s")
OBJ = ${SRC_C:.c=.o} ${SRC_ASM:.s=.o}

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename ${PWD})

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -I ./

all : ${OUT}

${OUT} : ${OBJ}
	${CC} ${LDFLAGS} -o ${OUT} ${OBJ}

%.o : %.c
	${CC} ${CFLAGS} -o $@ -c $^

clean : 
	rm -f ${OBJ}