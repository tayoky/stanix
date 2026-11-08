VERSION="4.2.2"
SOURCE="libmpfr"
DEPENDENCIES="libgmp"
BUILD_DEPENDENCIES="tool-autoconf tool-automake tool-libtool tool-stanix-toolchain"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.mpfr.org/"

configure () {
    gnu_configure --target="$HOST"
}
