VERSION="5.4.7"
SOURCE="lua"
DEPENDENCIES="stanix-base"

configure() {
	true
}

build() {
	make -C "$SOURCE_DIR" -j$NPROC
}

install() {
	make -C "$SOURCE_DIR" install INSTALL_TOP="$DESTDIR$PREFIX"
}