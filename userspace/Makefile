MAKEFLAGS += --no-builtin-rules

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# some programs want to be installed in /usr
%-tash   : PREFIX=/usr
%-tutils : PREFIX=/usr
%-tvu    : PREFIX=/usr
