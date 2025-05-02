#!/bin/sh

#build a the gcc/binutils toolchain

#first make sure the sanix stuff is configured
if [ ! -f config.mk ] ; then
	./configure --with-sysroot=$SYSROOT
fi

#copy the header
make header

mkdir toolchain && cd toolchain

#tdownload the source code
curl "https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz" > binutils.tar.xz
tar -xf bintuils.tar.xz

cd binutils

#apply patch
git apply ../../binutils.patch

#run automake in the ld directory
(cd ld && automake ) 

./configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --disable-werror
make
make install
cd ..

cd ..
./configure --host=$TAGRET --with-CC=$PREFIX/$TARGET-gcc --with-ld=$PREFIX/$TARGET-ld