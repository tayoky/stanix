VERSION="2.32.70"
SOURCE="sdl2-compat"
WEBSITE="https://libsdl.org/"
DEPENDENCIES="sdl3"

configure () {
	cmake_configure \
		-DSDL2COMPAT_X11=OFF -DSDL2COMPAT_TESTS=OFF -DSDL2COMPAT_VENDOR_INFO="Stanix"
}
