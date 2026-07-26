# makefile include to install includes

INCS ?= $(wildcard include/*)
FILESGROUPS += INCS

include $(TMAKE_DIR)/tmake-files.mk
