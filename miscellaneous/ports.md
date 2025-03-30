# ports
installing ports is simple  
first clone the repo with all the ports and patches :  
```sh
git clone https://github.com/tayoky/ports
```
then run the configure script with prefix set to the path to the initrd 
```sh
./configure --prefix=path/to/initrd
```
use build and install to install all port you want
```sh
./build.sh XXX
./install.sh XXX
```
just rebuild the initrd and the disk image and you're done !
