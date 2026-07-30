TOP ?= $(CURDIR)/../..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

LIB = trm
CFLAGS += -Iinclude

INCS = $(wildcard include/libtrm/*.h)
INCSDIR := $(INCSDIR)/libtrm

include $(TMAKE_DIR)/tmake-lib.mk
