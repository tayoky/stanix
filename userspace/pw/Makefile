TOP ?= $(CURDIR)/../..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

PROG = pw
include $(TMAKE_DIR)/tmake-prog.mk

CMDS = useradd usermod userdel

.PHONY : install-symlinks
install : install-symlinks
install-symlinks : install-bin
	$(Q)for I in $(CMDS) ; do \
		echo "LN_S pw $(DESTDIR)$(BINDIR)/$$I" ; \
		$(LN_S) -f "pw" "$(DESTDIR)$(BINDIR)/$$I" ; \
	done
