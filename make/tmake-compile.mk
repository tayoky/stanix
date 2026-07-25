# makefile include with rules to compile

ifneq ($(SRCDIRS),)
	vpath %.c $(SRCDIRS)
	vpath %.s $(SRCDIRS)
endif

$(BUILDDIR)/%.c.o : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -o "$@" -c $<

$(BUILDDIR)/%.c.so : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -fpic -D__SHARED__=1 -o "$@" -c $<

$(BUILDDIR)/%.s.o : %.s
	@mkdir -p "$(@D)"
	@echo "AS $<"
	$(Q)$(AS) $(ASFLAGS) -o "$@" -c $<

$(BUILDDIR)/%.s.so : %.s
	@mkdir -p "$(@D)"
	@echo "AS $<"
	$(Q)$(AS) $(ASFLAGS) -o "$@" -c $<
