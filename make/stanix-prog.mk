# makefile include to build a program

ifeq ($(V),1)
	Q =
else
	Q = @
endif

SRCS ?= $(wildcard *.[cs])
OBJS += $(SRCS:%=$(BUILDDIR)/%.o)
CFLAGS += -std=c99
CFLAGS += -I ./

all : $(BUILDDIR)/$(PROG)

$(BUILDDIR)/%.c.o : %.c
	@mkdir -p "$(@D)"
	@echo "CC $<"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(BUILDDIR)/%.s.o : %.s
	@mkdir -p "$(@D)"
	@echo "AS $<"
	$(Q)$(AS) $(ASFLAGS) -o $@ -c $<

$(BUILDDIR)/$(PROG) : $(OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(PROG)"
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install :
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	@echo "INSTALL $(PROG)"
	$(Q)cp "$(BUILDDIR)/$(PROG)" "$(DESTDIR)$(PREFIX)/bin/"

clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q) rm -rf "$(BUILDDIR)"

.PHONY : all install clean
