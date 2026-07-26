# makefile include to manage .mo/.po/.pot files

LOCALES ?= $(wildcard locale/*.po)
TEMPLATE_POT ?= $(BUILDDIR)/locale/template.pot
LOCALES_MO ?= $(LOCALES:%.po=$(BUILDDIR)/%.mo)
DOMAIN ?= $(PACKAGE)

ifneq ($(MULTI_LANGUAGES),no)
ifneq ($(strip $(LOCALES_MO)),)
all : $(LOCALES_MO)
$(BUILDDIR)/%.mo : %.po
	@mkdir -p "$(@D)"
	@echo "GEN $@"
	$(Q)msgfmt -o "$@" $^

.PHONY : install-locale
install : install-locale
install-locale : $(LOCALES_MO)
	@echo "INSTALL $(DOMAIN).mo"
	$(Q)for I in $(LOCALES_MO) ; do \
		LOC=$$(basename "$${I%%.mo}") ; \
		mkdir -p "$(LOCALEDIR)/$$LOC/LC_MESSAGES/" ; \
		cp "$$I" "$(LOCALEDIR)/$$LOC/LC_MESSAGES/$(DOMAIN).mo" ; \
	done

.PHONY : uninstall-locale
uninstall : uninstall-locale
uninstall-locale :
	@echo "UNINSTALL $(DOMAIN).mo"
	$(Q)for I in $(LOCALES) ; do \
		LOC=$$(basename "$${I%%.po}") ; \
		rm -f "$(LOCALEDIR)/$$LOC/LC_MESSAGES/$(DOMAIN).mo" ; \
	done
endif

.PHONY : update-po
update-po : $(TEMPLATE_POT)
$(TEMPLATE_POT) : $(SRCS)
	@mkdir -p "$(@D)"
	@echo "GEN $(TEMPLATE_POT)"
	$(Q)xgettext --keyword=_ --keyword=N_ \
		--package-name="$(PACKAGE)" \
		--package-version="$(VERSION)" \
		-o "$@" $^
	$(Q)for I in $(LOCALES) ; do \
		msgmerge --update $$I "$@"; \
	done
endif
