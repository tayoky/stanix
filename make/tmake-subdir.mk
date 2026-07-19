# makefile include to build subdirs

TARGETS = all install uninstall clean
SUBDIR_TARGETS = $(foreach DIR,$(SUBDIRS),$(addsuffix -$(DIR),$(TARGETS)))

.PHONY : $(TARGETS) $(SUBDIR_TARGETS)

$(TARGETS): % : $(addprefix %-, $(SUBDIRS))

$(SUBDIR_TARGETS): % :
	@DIR=$(patsubst $(firstword $(subst -, ,$*))-%,%,$*); \
	$(MAKE) -C $$DIR $(firstword $(subst -, ,$*)) BUILDDIR=$(BUILDDIR)/$$DIR
