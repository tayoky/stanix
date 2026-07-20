# makefile include to get version

VERSION := $(shell git describe --tags --always 2>/dev/null || echo unknown)
CFLAGS += -DVERSION='"$(VERSION)"'
