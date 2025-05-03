---
title: ports
---
installing ports is simple  
when you clone the stanix repo you get a ports folder   
just go into it and use build and install to install all ports you want
```sh
./build.sh XXX
./install.sh XXX
```
just rebuild the initrd and the disk image (using `make iso` or `make hdd`)  and you're done !

see [here](../packages) for a list of all the availables packages and ports
