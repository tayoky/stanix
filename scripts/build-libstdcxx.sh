#!/bin/sh

# build the libstdc++ library

# we need to make sure tlibc is built, as libstdc++ depends on libc
make build-tlibc || exit 1

GCC_VERSION=12.2.0

if ! cd toolchain/gcc-$GCC_VERSION/build ; then
    echo "the gcc toolchain need to be built before libstdc++"
    exit 1
fi

make -j $NPROC all-target-libstdc++-v3 || exit 1
make install-strip-target-libstdc++-v3 || exit 1
