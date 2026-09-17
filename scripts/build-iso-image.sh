# build an iso image from a sysroot


set -e
LIMINE_DIR="$BUILD_PREFIX/share/limine"
IMAGE_DIR="$BUILDDIR/images"
ISO_IMAGE="$IMAGE_DIR/stanix.iso"
mkdir -p "$IMAGE_DIR"

"$TOP/scripts/build-initrd.sh"

xorriso -as mkisofs -R -r -J -graft-points -b "/boot/limine/limine-bios-cd.bin"\
	-no-emul-boot -boot-load-size 4 -boot-info-table \
	-apm-block-size 2048 --efi-boot "/boot/limine/limine-uefi-cd.bin" \
	-efi-boot-part --efi-boot-image --protective-msdos-label \
	-V "STANIX" -copyright "/usr/share/doc/COPYING.txt" \
	"/boot/initrd.tar"="$BUILDDIR/initrd.tar" \
	"$SYSROOT" -o "$ISO_IMAGE"

limine bios-install "$ISO_IMAGE"