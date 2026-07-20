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
BINDIR     ?= $(PREFIX)/bin
SBINDIR    ?= $(PREFIX)/sbin
LIBDIR     ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
SHAREDIR   ?= $(PREFIX)/share
MANDIR     ?= $(SHAREDIR)/man
DOCDIR     ?= $(SHAREDIR)/doc
NLSDIR     ?= $(SHAREDIR)/nls
CONFDIR    ?= $(PREFIX)/etc
BUILDDIR  ?= $(TOP)/build$(CURDIR:$(abspath $(TOP))%=%)
TMAKE_DIR ?= $(TOP)/make

CFLAGS += -DPREFIX='"$(PREFIX)"'

ifeq ($(V),1)
	Q =
else
	Q = @
endif
