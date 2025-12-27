#!/bin/tash
echo "mounting /tmp"
mount -t tmpfs -S / -T /tmp
chmod 01777 /tmp
echo "mounting /proc"
mount -t proc -S / -T /proc
echo "mounting /sys"
mount -t sysfs -S / -T /sys
echo "mounting shmfs"
mkdir -p /dev/shm
mount -t shmfs -S / -T /dev/shm
chmod 01777 /dev/shm
chmod 0755 /dev
echo "loading modules"
insmod /mod/test.ko
insmod /mod/pci.ko
insmod /mod/nvme.ko
insmod /mod/ata.ko
insmod /mod/part.ko
insmod /mod/fat.ko
insmod /mod/8042.ko
insmod /mod/ps2-kb.ko
insmod /mod/serial.ko
echo "mount partitions"
automount

#setup font and frambuffer path
export FONT="/zap-light16.psf"
export FB="/dev/fb0"

#uncomment this line to get a shell on the serial port
#login --setup-stdin-from-stdout > /dev/ttyS0
term --layout azerty