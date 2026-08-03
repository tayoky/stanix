TOP ?= $(CURDIR)/../../../..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

MOD = ide
SRCS = ide.c ide_controller.c ide_channel.c
CFLAGS += -I.
include $(TMAKE_DIR)/stanix-mod.mk
