#!/bin/sh
# script to build the EFI System Partition (ESP)

set -e
LIMINE_DIR="$BUILD_PREFIX/share/limine"
ESP_ROOT="$BUILDDIR/esp"


mkdir -p "$ESP_ROOT/EFI/BOOT/"
for FILE in BOOTAA64.EFI BOOTIA32.EFI BOOTLOONGARCH64.EFI BOOTRISCV64.EFI BOOTX64.EFI ; do
    cp -Pf -p "$LIMINE_DIR/$FILE" "$ESP_ROOT/EFI/BOOT"
done

# copy boot files
cp -Pf -p -r "$SYSROOT/boot" "$ESP_ROOT"
cp "$TOP/limine.conf" "$ESP_ROOT/boot"

"$TOP/scripts/build-initrd.sh"
cd "$BUILDDIR/initrd" && tar -cf "$ESP_ROOT/boot/initrd.tar" *

