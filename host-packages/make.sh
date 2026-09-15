VERSION="4.4.1"
SOURCE="make"
WEBSITE="https://www.gnu.org/software/make"
DEPENDENCIES="stanix-base"

configure () {
	gnu_configure --without-guile --disable-job-server --disable-thread --disable-nls --disable-posix-spawn --enable-year-2038
}
