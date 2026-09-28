VERSION="3.14.7"
SOURCE="python"
BUILD_DEPENDENCIES="pkgconf"
WEBSITE="https://www.python.org/"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX" \
		--with-ensurepip="install" \
		--disable-ipv6 \
		--enable-shared \
		--disable-test-modules
}
