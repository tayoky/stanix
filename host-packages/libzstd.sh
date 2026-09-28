VERSION="0.4.2"
SOURCE="libzstd"
DEPENDENCIES="stanix-base"

configure () {
	true
}

build () {
	make -C "$SOURCE_DIR" default -j"$PARALLELISM"
}

install () {
	make -C "$SOURCE_DIR" install DESTDIR="$DESTDIR"
}
