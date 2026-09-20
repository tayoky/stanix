VERSION="0.0.1"
SOURCE="tlibc"
. "$TOP/scripts/tconf-package.sh"

# install first party packages in /usr
PREFIX=/usr

configure () {
	# give a fake compiler since the real compiler might not be ready yet
	"$SOURCE_DIR/configure" --builddir="$BUILD_DIR" --host="$HOST" --prefix="$PREFIX" \
		--cc=true --as=true --ar=true
}

build () {
	make all-include -j"$PARALLELISM"
}

install () {
	make install-include DESTDIR="$SYSROOT"
}
