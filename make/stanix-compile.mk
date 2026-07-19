# makefile include with rules to compile

$(BUILDDIR)/%.c.o : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -o "$@" -c $<

$(BUILDDIR)/%.c.so : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -fpic -o "$@" -c $<

$(BUILDDIR)/%.s.o : %.s
	@mkdir -p "$(@D)"
	@echo "AS $<"
	$(Q)$(AS) $(ASFLAGS) -o "$@" -c $<
