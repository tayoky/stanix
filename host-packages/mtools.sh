VERSION="4.0.49"
SOURCE="mtools"
DEPENDENCIES="stanix-base"
. "$TOP/scripts/gnu-package.sh"
WEBSITE="https://www.gnu.org/software/mtools/"

configure () {
	# tlibc's iconv.h is kind of broken
	gnu_configure CFLAGS=-Wno-error ac_cv_header_iconv_h=no
}

install () {
	make install DESTDIR="$DESTDIR"
}
