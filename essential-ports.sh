#!/bin/sh
#built all the essential ports
cd ports
./clean.sh tsh
./clean.sh tutils
./build.sh tsh
./build.sh tutils
./install.sh tsh
./install.sh tutils