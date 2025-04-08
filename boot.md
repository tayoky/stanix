---
title: boot process
---
this is how the OS boot  

> [!NOTE]
> this is the new boot process it isen't implemented

first the **BIOS**/**UEFI** load the **bootloader**

### bootloader
the **bootloader** load the **kernel**(stanix.elf) and the **initial ramdisk**(initrd.tar)  
it also pass information such as the **memmory map** and the **framebuffers locations** to the **kernel**  

### kernel
at this stage stanix as started and the **kernel** initialize core things and arch specific such as the **bitmap allocator** or the **exception handler**  
once it as inizialed the **VFS** it will uncompress the **initial ramdisk** on a **tmpfs** and mount it as root  
the **kernel** will then read config files and start the scheduler then the **init userspace program**(/bin/init)

### init userspace program
the **init program** is the first userspace program to run , compared to most program it is on the **initial ramdisk** and not on a normal fat32 or ext2 partition  
it will run in a space call the **early userspace** wich is like normal userspace except most kernel driver ar not loaded and the filesystem as not been setup  
the **init program** will run a serie of script call the **init scripts**

### init script 1 - module loading
the first **init script** (`/init/1-modules.sh`) will load all filesystem and disk driver (these kernel modules are the only on the **inital ramdisk**)

### init script 2 - setup root
the second **init script**(`/init/2-root.sh`) will scan the boot options to choise the good partition and mount it under `/root`
then it will symlink `/root/initrd` to `/` (to keep acces to the **initial ramdisk**) and chroot into `/root`

### init script 3 - migrate
the third **init script** (`/init/3-migrate.sh`) will migrate `/initrd/bin` , `/initrd/mod` and `/initrd/dev` to `/bin` , `/mod` and `/dev`

### init script 4 - startup apps
the fourth **init script** (`/init/4-start.s`) will read boot options and start application (such as `term) with the specified arg

## term and login
term will emulate an pty and start login , login on the other hand is started on serial port or on pty emulated by term and will show a nice login for you to connect  
login will check your password in `/etc/master.passpwd` and lauch your shell (like tsh)
