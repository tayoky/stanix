all : test

test.c.o : test.c
	tcc -c -o $@ $^

test : test.c.o
	tcc -o $@ $^