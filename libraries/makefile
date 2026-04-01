MAKEFLAGS += --no-builtin-rules
PREFIX = $(SYSROOT)/usr

ifeq ($(TOP),)
CFLAGS += -O2 -g
LDFLAGS += -g
export CFLAGS
endif
export PREFIX

#first get all the sub directory
LIBS = $(shell find -name "*" -not -path "." -type d -maxdepth 1)

all : $(foreach LIB,$(LIBS), $(LIB)-all)
install : $(foreach LIB,$(LIBS), $(LIB)-install)
clean : $(foreach LIB,$(LIBS), $(LIB)-clean)


# depencies
libtgui-all : libgfx-install libtwm-install

%-all :
	@$(MAKE) -C $(@:%-all=%)
%-install : %-all
	@$(MAKE) -C $(@:%-install=%) install
%-clean :
	@$(MAKE) -C $(@:%-clean=%) clean

.PHONY : all install clean
