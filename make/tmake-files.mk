# makefile include to install files

.PHONY : install-files uninstall-files

install : install-files
install-files :
	@mkdir -p "$(DESTDIR)$(FILESDIR)"
	@echo "INSTALL $(FILES)"
	$(Q)cp -r $(FILES) "$(DESTDIR)$(FILESDIR)/"

uninstall : uninstall-files
uninstall-files :
	@echo "UNINSTALL $(FILES)"
	$(Q)rm -fr $(addprefix $(DESTDIR)$(FILESDIR)/,$(FILES))
