#!/bin/sh

# build the gcc/binutils toolchain

# default
: ${SYSROOT:="./sysroot"}
: ${TARGET:="$(uname -m)-stanix"}
: ${NPROC:=$(nproc)}

for i in "$@"; do
	case $i in
		--sysroot=*)
			SYSROOT="${i#*=}"
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


# make absolute path
SYSROOT="$(realpath "$SYSROOT")"

ARCH=${TARGET%%-*}

# save the path to the top
TOP="$PWD"

PREFIX="$TOP/toolchain"
set -e

# put everything inside toolchain
mkdir -p toolchain/bin
cd toolchain

# pre export the toolchain/bin in PATH
# as gcc might require binutils stuff to be in the PATH
export PATH="$PWD/bin:$PATH"

# download the archive
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
echo "making cross toolchain for binutils $BINUTILS_VERSION and gcc $GCC_VERSION ..."


if ! test -e binutils-$BINUTILS_VERSION.tar.xz ; then
	echo "downloading binutils..."
	$WGET -Obinutils-$BINUTILS_VERSION.tar.xz "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi
if ! test -e gcc-$GCC_VERSION.tar.xz ; then
	echo "downloading gcc..."
	$WGET -Ogcc-$GCC_VERSION.tar.xz "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi
if ! test -d binutils-$BINUTILS_VERSION ; then
	echo "unpacking binutils..."
	tar xf binutils-$BINUTILS_VERSION.tar.xz
fi
if ! test -d gcc-$GCC_VERSION ; then
	echo "unpacking gcc..."
	tar xf gcc-$GCC_VERSION.tar.xz
fi

# installed autoconf version
AUTOCONF_VERSION=$(autoconf --version | grep -E -o "2\.((69)|(7[1-3]))")
if test -z "$AUTOCONF_VERSION" ; then
	echo "invalid autoconf version"
	exit 1
fi
AUTOMAKE_VERSION=$(automake --version | grep -E -o "1\.((16)|(15))\.[0-9]+")
if test -z "$AUTOMAKE_VERSION" ; then
	echo "invalid automake version"
	exit 1
fi
echo "found autoconf $AUTOCONF_VERSION"
echo "found automake $AUTOMAKE_VERSION"

# now apply the patch and run automake if not aready done
if ! test -e binutils-$BINUTILS_VERSION/ld/emulparams/elf_x86_64_stanix.sh ; then
	patch -ruN -p1 -d binutils-$BINUTILS_VERSION -i $TOP/binutils.patch
	sed -i -e "s/2.69/$AUTOCONF_VERSION/g" binutils-$BINUTILS_VERSION/config/override.m4
	sed -i -e "s/1.15.1/$AUTOMAKE_VERSION/g" binutils-$BINUTILS_VERSION/ld/aclocal.m4
	cd binutils-$BINUTILS_VERSION/
	autoreconf -fiv
	(cd ld && automake)
	cd ..
fi
if ! test -e gcc-$GCC_VERSION/gcc/config/stanix.h ; then
	patch -ruN -p1 -d gcc-$GCC_VERSION -i $TOP/gcc.patch
	sed -i -e "s/2.69/$AUTOCONF_VERSION/g" gcc-$GCC_VERSION/config/override.m4
	sed -i -e "s/1.15.1/$AUTOMAKE_VERSION/g" gcc-$GCC_VERSION/libstdc++-v3/aclocal.m4
	cd gcc-$GCC_VERSION/
	autoreconf -fiv
	(cd libstdc++-v3 && automake)
	cp "config.sub~" config.sub
	cd ..
fi

# we are going to need the header
if ! test -e ../tlibc/configure ; then
	git submodule init
	git submodule update ../tlibc
fi
make -C "$TOP" header PREFIX="/usr" SYSROOT="$SYSROOT" ARCH="$ARCH"

# now compile all the shit
# don't compile if aready done

# build binutils
if ! test -e bin/$TARGET-ld ; then
	cd binutils-$BINUTILS_VERSION
	if ! test -f build/Makefile ; then
		mkdir -p build && cd build
		echo "configure binutils..."
		../configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --disable-werror --enable-shared
	else
		cd build
	fi
	echo "building binutils..."
	make -j$NPROC
	echo "installing binutils..."
	make install-strip
	cd ../..
fi

# build bootstrap gcc
if ! test -e bootstrap/bin/$TARGET-gcc ; then
	cd gcc-$GCC_VERSION
	if ! test -f build-bootstrap/Makefile ; then
		mkdir -p build-bootstrap && cd build-bootstrap
		echo "configure bootstrap gcc..."
		../configure --target=$TARGET --prefix="$PREFIX/bootstrap" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c --without-headers --disable-shared --disable-multilib \
			--disable-libgomp --disable-libssp --disable-libquadmath --disable-libsanitizer \
			--disable-decimal-float --disable-bootstrap
	else
		cd build-bootstrap
	fi
	echo "building bootstrap gcc..."
	make all-gcc -j$NPROC
	make all-target-libgcc -j$NPROC
	echo "installing bootstrap gcc..."
	make install-strip-gcc
	make install-strip-target-libgcc

	cd ../..
fi

# build final gcc
if ! test -e bin/$TARGET-gcc ; then
	# we need to make sure we have a libc
	echo "configure bootstrap tlibc ..."
	(
		export PATH="$PREFIX/bootstrap/bin:$PATH" 
		cd "$TOP/tlibc"
		./configure --host="$TARGET" --enable-shared --prefix="/usr"
	)

	echo "building bootstrap tlibc..."
	make -C "$TOP/tlibc" install-libc DESTDIR="$SYSROOT" -j$NPROC

	cd gcc-$GCC_VERSION
	if ! test -f build/Makefile ; then
		mkdir -p build && cd build
		echo "configure gcc..."
		../configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --enable-languages=c,c++ --disable-multilib --enable-shared --with-pic --enable-threads=posix
	else
		cd build
	fi
	echo "building gcc..."
	make all-gcc -j$NPROC
	make all-target-libgcc -j$NPROC
	echo "installing gcc..."
	make install-strip-gcc
	make install-target-libgcc

	cd ../..
fi

# copy libgcc to sysroot
mkdir -p "$SYSROOT/usr/lib"
cp "$TOP/toolchain/$TARGET/lib/libgcc_s.so" "$SYSROOT/usr/lib"
cp "$TOP/toolchain/$TARGET/lib/libgcc_s.so.1" "$SYSROOT/usr/lib"
