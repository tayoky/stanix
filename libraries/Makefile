MAKEFLAGS += --no-builtin-rules

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include stanix-subdir.mk

# dependencies
all-libtgui : install-libgfx install-libtwm
all-libini  : install-libutils
all-libtwm  : install-libgfx
