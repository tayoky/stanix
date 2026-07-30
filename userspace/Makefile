TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

export BINDIR

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# we want to install somes binaries in /bin
install-tash   : BINDIR=/bin
install-tutils : BINDIR=/bin
install-init   : BINDIR=/bin
install-getty  : BINDIR=/bin
