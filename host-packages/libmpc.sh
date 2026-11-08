VERSION="1.4.1"
SOURCE="libmpc"
DEPENDENCIES="libgmp libmpfr"
BUILD_DEPENDENCIES="tool-autoconf tool-automake tool-libtool tool-stanix-toolchain"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.multiprecision.org/"

configure () {
    gnu_configure --target="$HOST" --with-sysroot="$SYSROOT"
}
