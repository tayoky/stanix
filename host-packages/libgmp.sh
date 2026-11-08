VERSION="6.3.0"
SOURCE="libgmp"
DEPENDENCIES="stanix-base"
BUILD_DEPENDENCIES="tool-autoconf tool-automake tool-libtool tool-stanix-toolchain"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://gmplib.org/"

configure () {
    gnu_configure --disable-cxx
}
