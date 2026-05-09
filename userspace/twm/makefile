
#first get all the src
SRC = $(shell find -name "*.c")
OBJ = $(SRC:%.c=$(BUILDDIR)/%.o)

#find bin name
PWD = $(shell pwd)
OUT = $(shell basename $(PWD))

#cc flags
CFLAGS += -Wall -Wextra -std=c99
CFLAGS += -I ./

LDFLAGS += -lgfx -linput

all : $(BUILDDIR)/$(OUT)

$(BUILDDIR)/$(OUT) : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean : 
	rm -f $(BUILDDIR)
