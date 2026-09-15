VERSION="2.32.70"
SOURCE="sdl2-compat"
DEPENDENCIES="sdl3"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure \
		-DSDL2COMPAT_X11=OFF -DSDL2COMPAT_TESTS=OFF -DSDL2COMPAT_VENDOR_INFO="Stanix"
}
