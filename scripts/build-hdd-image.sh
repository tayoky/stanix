#!/bin/sh
# build an hdd image from a sysroot

set -e
IMAGE_DIR="$BUILDDIR/images"
HDD_IMAGE="$IMAGE_DIR/stanix.hdd"
mkdir -p "$IMAGE_DIR"

"$TOP/scripts/build-esp.sh"

: ${HEAD_SIZE:=1}
: ${ESP_SIZE:=10}
: ${EXT2_SIZE:=32}

: ${ESP_ROOT:="$BUILDDIR/esp"}
: ${EXT2_ROOT:="$SYSROOT"}

: ${HEAD_IMAGE:="$BUILDDIR/head.img"}
: ${ESP_IMAGE:="$BUILDDIR/esp.img"}
: ${EXT2_IMAGE:="$BUILDDIR/ext2.img"}

dd if=/dev/zero of="$ESP_IMAGE" bs=1M count=$ESP_SIZE
mformat -i "$ESP_IMAGE" -v "EFI" ::
mcopy -i "$ESP_IMAGE" -s "$ESP_ROOT"/* ::/

dd if=/dev/zero of="$EXT2_IMAGE" bs=1M count=$EXT2_SIZE
mke2fs -t ext2 -d "$EXT2_ROOT" -F "$EXT2_IMAGE"

dd if=/dev/zero of="$HEAD_IMAGE" bs=1M count=$HEAD_SIZE

cat "$HEAD_IMAGE" "$ESP_IMAGE" "$EXT2_IMAGE" > "$HDD_IMAGE"
sgdisk -n 1:2048:+${ESP_SIZE}M -t 1:ef00 -c 1:"EFI System Partition" "$HDD_IMAGE"
sgdisk -n 2:0:0 -t 2:8300 -c 2:"Stanix" "$HDD_IMAGE"

limine install-bios "$HDD_IMAGE"
