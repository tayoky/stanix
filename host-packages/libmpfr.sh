VERSION="4.2.2"
SOURCE="libmpfr"
DEPENDENCIES="libgmp"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.mpfr.org/"

configure () {
    gnu_configure --target="$HOST"
}
