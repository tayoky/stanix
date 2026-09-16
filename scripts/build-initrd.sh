#!/bin/sh
# script to build the inital ramdisk (initrd)

set -e
INITRD="$BUILDDIR/initrd"
BASE_INITRD="$TOP/base/initrd"

mkdir -p "$INITRD/dev" "$INITRD/tmp" "$INITRD/mnt"
cp -Pf -p -r "$BASE_INITRD"/* "$INITRD/"
cp -Pf -p -r "$SYSROOT/mod" "$INITRD"
# temporary until real sysroot, copy sysroot to initrd
cp -Pf -p -r "$SYSROOT"/* "$INITRD/"
