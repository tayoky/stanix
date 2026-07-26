# makefile include with default values

MAKEFLAGS += --no-builtin-rules

TOP ?= $(CURDIR)
-include $(TOP)/config.mk

CC ?= cc
LD ?= ld
AS ?= as
LN ?= ln
LN_S ?= ln -s
INSTALL ?= install
PREFIX ?= /usr/local
BINDIR    ?= $(PREFIX)/bin
SBINDIR   ?= $(PREFIX)/sbin
LIBDIR    ?= $(PREFIX)/lib
INCSDIR   ?= $(PREFIX)/include
SHAREDIR  ?= $(PREFIX)/share
MANDIR    ?= $(SHAREDIR)/man
DOCDIR    ?= $(SHAREDIR)/doc
NLSDIR    ?= $(SHAREDIR)/nls
LOCALEDIR ?= $(SHAREDIR)/locale
CONFDIR   ?= $(PREFIX)/etc
TMAKE_DIR ?= $(TOP)/make
STAMP     ?= $(BUILDDIR)/.stamp

ifeq ($(BUILDDIR),)
	BUILDDIR  ?= $(TOP)/build$(CURDIR:$(abspath $(TOP))%=%)
else
	BUILDDIR  := $(BUILDDIR)$(CURDIR:$(abspath $(TOP))%=%)
endif

CFLAGS += -DPREFIX='"$(PREFIX)"'
CFLAGS += -DLOCALEDIR='"$(LOCALEDIR)"'

ifeq ($(V),1)
	Q =
else
	Q = @
endif

# define mains targets
all :
.PHONY : all
