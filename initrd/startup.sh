#!/bin/tsh
echo "mounting /tmp"
mount -t tmpfs -S / -T /tmp
echo "loading modules"
insmod /mod/test.ko
insmod /mod/pci.ko
insmod /mod/nvme.ko
insmod /mod/8042.ko
insmod /mod/ps2-kb.ko
insmod /mod/serial.ko
#uncomment this line to get a shell on the serial port
#login --setup-stdin-from-stdout > /dev/ttyS0
term