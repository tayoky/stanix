VERSION="6.3.0"
SOURCE="libgmp"
DEPENDENCIES="stanix-base"
BUILD_DEPENDENCIES="libtool"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://gmplib.org/"

configure () {
    gnu_configure --disable-cxx
}
