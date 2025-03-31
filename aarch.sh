#!/bin/sh
make OVMF-img.bin
qemu-system-aarch64 -cpu cortex-a76 -M virt -m 512m -usb -device qemu-xhci -device usb-kbd -device usb-mouse -device ramfb -device ahci,id=ahci -drive if=pflash,unit=0,format=raw,file=OVMF-img.bin -cdrom stanix.iso