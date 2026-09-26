VERSION="5.4.7"
SOURCE="lua"
DEPENDENCIES="stanix-base"

configure() {
	true
}

build() {
	make -C "$SOURCE_DIR" -j$NPROC \
		CC="$HOST-gcc" AR="$HOST-ar rcu"
}

install() {
	make -C "$SOURCE_DIR" install INSTALL_TOP="$DESTDIR$PREFIX" \
		CC="$HOST-gcc" AR="$HOST-ar rcu"
}
