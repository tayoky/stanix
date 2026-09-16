VERSION="0.4.2"
SOURCE="libzstd"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	make default -j"$PARALLELISM"
}

install () {
	make install DESTDIR="$DESTDIR"
}
