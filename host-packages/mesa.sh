VERSION="25.0.7"
SOURCE="mesa"
DEPENDENCIES="zlib"
. "$TOP/scripts/meson-package.sh"
WEBSITE="https://mesa3d.org"

configure () {
	CFLAGS="-Wno-error -Wno-implicit-function-declaration" \
		meson_configure \
		-D platforms=[] \
		-D gallium-drivers=softpipe  \
		-D vulkan-drivers=[]   \
		-D valgrind=disabled \
		-D glx=disabled \
		-D osmesa=true \
		-D llvm=disabled 
}
