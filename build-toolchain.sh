#!/bin/sh

#build a the tcc toolchain

#save the path to the top and some files
TOP=$PWD
PATCH="$PWD/ports/ports/tcc/tcc.patch"
mkdir -p toolchain
cd toolchain

git clone https://github.com/tinycc/tinycc.git --depth 1 --single-branch
cd tinycc

#make sure the ports subomdules is here
if [ ! -e "$PATCH" ] ; then
	git submodule init
	git submodule update ports
fi

git apply $PATCH

./configure --prefix=$TOP/toolchain --targetos=stanix --sysroot=$1 --enable-static
make
make install

cd $TOP

echo "#generated automaticly by ./build-toolchain.sh" > add-to-path.sh
echo "export PATH=$TOP/toolchain/bin\$PATH" >> add-to-path.sh
echo "export CC=tcc" >> add-to-path.sh
echo "export LD=tcc" >> add-to-path.sh
echo "export AR=\"tcc -ar\"" >> add-to-path.sh

chmod +x add-to-path.sh

echo "please run '''. add-to-path.sh''' before compiling anything"