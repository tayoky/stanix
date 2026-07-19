# makefile include to install files

.PHONY : install-files uninstall-files

install : install-files
install-files :
	@mkdir -p "$(DESTDIR)$(PREFIX)$(FILESDIR)"
	@echo "INSTALL $(FILES)"
	$(Q)cp -r $(FILES) "$(DESTDIR)$(PREFIX)$(FILESDIR)/"

uninstall : uninstall-files
uninstall-files :
	@echo "UNINSTALL $(FILES)"
	$(Q)rm -fr $(addprefix $(DESTDIR)$(PREFIX)$(FILESDIR)/,$(FILES))
