#!/bin/sh

#build the gcc/binutils toolchain

#default
SYSROOT="./sysroot"
TARGET="$(uname -m)-stanix"
NPROC=$(nproc)

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
    --target=*)
      TARGET="${i#*=}"
      shift # past argument=value
      ;;
    --nproc=*)
      NPROC="${i#*=}"
      shift # past argument=value
      ;;
    --help)
      echo "./build-toolchain.sh [OPTIONS]"
      echo "options :"
      echo "--help : show this help"
      echo "--target : target of the toolchain [$TARGET]"
      echo "--cc : C compiler to compile the toolchain"
      echo "--ar : archiver to use to compile the toolchain"
      echo "--sysroot : path to sysroot for include/libray path [$(realpath $SYSROOT)]"
      echo "--nproc : -j option for makefile [$NPROC]"
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

#make absolute path
SYSROOT="$(realpath "$SYSROOT")"

ARCH=${TARGET%%-*}

#save the path to the top
TOP=$PWD

PREFIX=$TOP/toolchain

#put everything inside toolchain
mkdir -p toolchain/bin
cd toolchain

#pre export the toolchain/bin in PATH
#as gcc might require binutils stuff to be in the PATH
export PATH="$PWD/bin:$PATH"

#download the archive
if wget --version > /dev/null 2> /dev/null ; then
  WGET="wget"
elif curl --version > /dev/null 2> /dev/null ; then
  WGET="curl"
else
  echo "error : curl and wget are not installed, please install one of the two"
  exit 1
fi


BINUTILS_VERSION=2.44
GCC_VERSION=12.2.0
if [ ! -e binutils.tar.xz ] ; then
  $WGET -Obinutils.tar.xz "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi
if [ ! -e gcc.tar.xz ] ; then
  $WGET -Ogcc.tar.xz "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi
if [ ! -d binutils-$BINUTILS_VERSION ] ; then
  tar xf binutils.tar.xz
fi
if [ ! -d gcc-$GCC_VERSION ] ; then
  tar xf gcc.tar.xz
fi

#put your autoconf version below
AUTOCONF_VERSION=2.71

#now apply the patch and run automake if not aready done
if [ ! -e gcc-$GCC_VERSION/gcc/config/stanix.h ] ; then
  patch -ruN -p1 -d gcc-$GCC_VERSION -i $TOP/gcc.patch
  sed -i -e "s/2.69/$AUTOCONF_VERSION/g" gcc-$GCC_VERSION/config/override.m4
  cd gcc-$GCC_VERSION/libstdc++-v3
  autoreconf
  automake
  cd ../..
fi
if [ ! -e binutils-$BINUTILS_VERSION/ld/emulparams/elf_x86_64_stanix.sh ] ; then
  patch -ruN -p1 -d binutils-$BINUTILS_VERSION -i $TOP/binutils.patch
  sed -i -e "s/2.69/$AUTOCONF_VERSION/g" binutils-$BINUTILS_VERSION/config/override.m4
  cd binutils-$BINUTILS_VERSION/ld
  autoreconf
  automake
  cd ../..
fi

#we are going to need the header
if [ ! -e ../tlibc/configure ] ; then
  git submodule init
  git submodule update ../tlibc
fi
make -C ../tlibc header PREFIX=$SYSROOT/usr TARGET=stanix ARCH=$ARCH

#now compile all the shit
#don't compile if aready done
if [ ! -e bin/$TARGET-ld ] ; then
  cd binutils-$BINUTILS_VERSION
  ./configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --disable-werror --enable-shared
  make -j$NPROC
  make install
  cd ..
fi
if [ ! -e bin/$TARGET-gcc ] ; then
  cd gcc-$GCC_VERSION
  mkdir -p build && cd build
  ../configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c,c++ --disable-hosted-libstdcxx --disable-multilib
  make all-gcc -j$NPROC
  make all-target-libgcc -j$NPROC
  make install-gcc
  make install-target-libgcc
  cd ../..
fi

cd $TOP

echo "#generated automaticly by ./build-toolchain.sh" > add-to-path.sh
echo "export PATH=$TOP/toolchain/bin:\$PATH" >> add-to-path.sh
echo "export CC=$TARGET-gcc" >> add-to-path.sh
echo "export LD=$TARGET-ld" >> add-to-path.sh
echo "export AS=$TARGET-as" >> add-to-path.sh
echo "export AR=$TARGET-ar" >> add-to-path.sh

chmod +x add-to-path.sh

echo "please run '''. add-to-path.sh''' before compiling anything"
