VERSION="0.0.1"
SOURCE="tlibc"
. "$TOP/scripts/tconf-package.sh"

# install first party packages in /usr
PREFIX=/usr

build () {
	make all-include -j"$PARALLELISM"
}

install () {
	make install-include DESTDIR="$SYSROOT"
}
