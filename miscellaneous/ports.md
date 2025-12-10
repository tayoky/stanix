---
title: ports
---
Installing ports is simple.  
When you clone the stanix repo you get a ports folder, once stanix is built just go into it and use build and install to install all ports you want.
```sh
./build.sh XXX
./install.sh XXX
```
where XXX is the name of the port.
Rebuild the initrd and the disk image (using `make iso` or `make hdd`) and you're done !

See [here](../packages) for a list of all the availables packages and ports.
