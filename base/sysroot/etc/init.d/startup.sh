#!/bin/tash
echo "mounting /tmp"
mount -t tmpfs -S / -T /tmp
chmod 01777 /tmp
echo "mounting /proc"
mount -t proc -S /dev/null -T /proc
echo "mounting /sys"
mount -t sysfs -S /dev/null -T /sys
echo "mounting shmfs"
mkdir -p /dev/shm
mount -t tmpfs -S /dev/null -T /dev/shm
chmod 01777 /dev/shm
chmod 0755 /dev
echo "loading modules"

for MODULE in \
pci \
libscsi scsi-mmc \
libata ide ata atapi \
mbr gpt fat iso9660 \
libps2 i8042 ps2-kb ps2-mouse \
isa \
serial ; do
    if kcmdline "--disable-$MODULE" ; then
        echo "skiped loading of $MODULE.ko"
        continue
    fi
    insmod "/mod/$MODULE.ko"
done

echo "mount partitions"
automount

# setup font and frambuffer path
export FONT="/usr/share/fonts/zap-light16.psf"
export FB="/dev/fb0"

# change this to change the keyboard layout
set-layout /dev/kb0 azerty

# we don't have an audio driver so setup sdl to use dummy audio
export SDL_AUDIODRIVER="dummy"

if kcmdline --gui --twm ; then
    twm
else
    fbterm --autologin root "$FB"
fi
