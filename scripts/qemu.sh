#!/bin/sh

help () {
	echo "usage : $0 [OPTIONS]"
	echo "launch an image in qemu"
	echo "options :"
	echo "--dont-build-image  : do not build the image"
	echo "--iso-image         : use the iso image"
	echo "--hdd-image         : use the hdd image"
	echo "--memory=MEMORY     : specify the amount of memory [$MEMORY]"
	echo "--media=MEDIA       : specify the media type, values can be nvme,ata,cdrom [$MEDIA]"
	echo "--stdio-serial      : redirect serial to stdio"
	echo "--kvm               : use kvm"
	echo "--extra-options=XXX : pass extra options direcly to qemu"
	echo "--qemu=QEMU         : specify the qemu to use [$QEMU]"
	echo "--help              : show this help and exit"
}

error () {
	echo "qemu.sh : $@" >&2
}

add_options () {
	INTERNAL_OPTIONS="$INTERNAL_OPTIONS $@"
}

set -e

: ${HOST:="$(uname -m)-stanix"}
: ${ARCH:="${HOST%%-*}"}
: ${QEMU:="qemu-system-$ARCH"}
IMAGE_TYPE="iso"
BUILD_IMAGE="yes"
MEMORY="512"
EXTRA_OPTIONS=""
INTERNAL_OPTIONS=""
MEDIA="nvme"

for I in "$@" ; do
	case "$I" in
		--dont-build-image)
			BUILD_IMAGE="no"
			;;
		--iso-image|--hdd-image)
			TMP="${I#--}"
			IMAGE_TYPE="${TMP%-image}"
			;;
		--memory=*)
			MEMORY="${I#*=}"
			;;
		--media=*)
			MEDIA="${I#*=}"
			;;
		--stdio-serial)
			add_options -serial stdio
			;;
		--kvm)
			add_options -cpu host -enable-kvm -smp 1
			;;
		--extra-options=*)
			EXTRA_OPTIONS="${I#*=}"
			;;
		--qemu=*)
			QEMU="${I#*=}"
			;;
		--help)
			help
			exit 0
			;;
		--*)
			error "invalid option $I"
			exit 1
			;;
	esac
done

IMAGE="$BUILDDIR/images/stanix.$IMAGE_TYPE"

if test "$BUILD_IMAGE" = "yes" ; then
	"$TOP/scripts/build-$IMAGE_TYPE-image.sh"
fi


case "$MEDIA" in
	nvme)
		add_options -drive file="$IMAGE",format=raw,if=none,id=nvm \
			-device nvme,serial=deadbeef,drive=nvm
		;;
	ata)
		add_options -drive file="$IMAGE",format=raw,if=ide
		;;
	cdrom|cd|atapi)
		add_options -cdrom "$IMAGE"
		;;
	*)
		error "invalid media $MEDIA"
		exit 1
		;;
esac

add_options -m "$MEMORY"
add_options $EXTRA_OPTIONS

"$QEMU" $INTERNAL_OPTIONS
