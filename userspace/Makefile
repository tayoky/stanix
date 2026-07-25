TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# we want to install tutils and tash binaries in /bin
install-tash   : BINDIR=/bin
install-tutils : BINDIR=/bin
