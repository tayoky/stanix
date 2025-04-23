#!/bin/tsh
echo "loading modules"
insmod /mod/test.ko
insmod /mod/pci.ko
insmod /mod/nvme.ko
insmod /mod/8042.ko
insmod /mod/ps2-kb.ko
term