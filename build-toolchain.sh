#!/bin/sh

#build a the tcc toolchain

#default
SYSROOT="./sysroot"
TARGET="$(uname -m)-stanix"

for i in "$@"; do
  case $i in
    --sysroot=*)
      SYSROOT="${i#*=}"
      shift # past argument=value
      ;;
    --cc=*)
      CC="${i#*=}"
      shift # past argument=value
      ;;
    --ar=*)
      AR="${i#*=}"
      shift # past argument=value
      ;;
    --host=*)
      HOST="${i#*=}"
      shift # past argument=value
      ;;
    --help)
      echo "./build-toolchain.sh [OPTIONS]"
      echo "options :"
      echo "--help : show this help"
      echo "--target : target of the toolchain [$TARGET]"
      echo "--cc : C compiler to compile the toolchain"
      echo "--ar : archiver to use to compile the toolchain"
      echo "--sysroot : path to sysroot for include/libray path [$SYSROOT]"
      exit
      ;;
    -*|--*)
      echo "Unknown option $i"
      exit 1
      ;;
    *)
      ;;
  esac
done

ARCH=${TARGET%%-*}

#save the path to the top and some files
TOP=$PWD
PATCH="$PWD/ports/ports/tcc/tcc.patch"

#make sure the ports subomdules is here
if [ ! -e "$PATCH" ] ; then
	git submodule init
	git submodule update ports
fi

#clone the repo
mkdir -p toolchain
cd toolchain
git clone https://github.com/tinycc/tinycc.git --depth 1 --single-branch
cd tinycc

git apply $PATCH

./configure --prefix=$TOP/toolchain --targetos=stanix --cpu=$ARCH --sysroot=$SYSROOT --enable-static
make
make install

#make symlink for XXX-stanix-tcc and stuff
cd $TOP/toolchain/bin
ln -s tcc $TARGET-tcc

#we make XXX-stanix-as and ld
#so that configure script that search will find it
ln -s tcc $TARGET-as
ln -s tcc $TARGET-ld

cd $TOP

echo "#generated automaticly by ./build-toolchain.sh" > add-to-path.sh
echo "export PATH=$TOP/toolchain/bin:\$PATH" >> add-to-path.sh
echo "export CC=$TARGET-tcc" >> add-to-path.sh
echo "export LD=$TARGET-tcc" >> add-to-path.sh
echo "export AS=$TARGET-tcc" >> add-to-path.sh
echo "export AR=\"$TARGET-tcc -ar\"" >> add-to-path.sh

chmod +x add-to-path.sh

echo "please run '''. add-to-path.sh''' before compiling anything"
