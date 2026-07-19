MAKEFLAGS += --no-builtin-rules

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include stanix-subdir.mk
