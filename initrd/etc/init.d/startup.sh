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
mount -t tmpfs -S / -T /dev/shm
chmod 01777 /dev/shm
chmod 0755 /dev
echo "loading modules"

for MODULE in test \
pci \
nvme ata \
part fat \
i8042 ps2-kb ps2-mouse \
serial ; do
    if kcmdline "--disable-$MODULE" ; then
        echo "skiped loading of $MODULE.ko"
        continue
    fi
    insmod "/mod/$MODULE.ko"
done

echo "mount partitions"
automount

#setup font and frambuffer path
export FONT="/usr/share/fonts/zap-light16.psf"
export FB="/dev/fb0"

# change this to change the keyboard layout
set-layout /dev/kb0 azerty

#uncomment this line to get a shell on the serial port
#login --setup-stdin-from-stdout > /dev/ttyS0

if kcmdline --gui --twm ; then
    twm
else
    fbterm
fi
