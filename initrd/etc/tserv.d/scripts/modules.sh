#!/bin/sh
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
