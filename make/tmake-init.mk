# makefile include with default values

MAKEFLAGS += --no-builtin-rules

TOP ?= $(CURDIR)
-include $(TOP)/config.mk

CC ?= cc
LD ?= ld
AS ?= as
LN ?= ln
LN_S ?= ln -s
PREFIX ?= /usr/local
BUILDDIR ?= $(TOP)/build$(CURDIR:$(abspath $(TOP))%=%)
TMAKE_DIR ?= $(TOP)/make

CFLAGS += -DPREFIX='"$(PREFIX)"'

ifeq ($(V),1)
	Q =
else
	Q = @
endif
