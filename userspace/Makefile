TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# some programs want to be installed in /usr
%-tash   : PREFIX=/usr
%-tutils : PREFIX=/usr
%-tvu    : PREFIX=/usr
