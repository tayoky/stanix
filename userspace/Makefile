MAKEFLAGS += --no-builtin-rules
PREFIX = /

# first get all sub directories
SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include stanix-subdir.mk

# some programs want to be installed in /usr
%-tash   : PREFIX=/usr
%-tutils : PREFIX=/usr
%-tvu    : PREFIX=/usr
