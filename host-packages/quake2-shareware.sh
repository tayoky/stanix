VERSION="3.14"
QUAKE_EXE="q2-314-demo-x86.exe"
DEPENDENCIES="quake2generic"

configure () {
	tinx_download "https://www.gamers.org/pub/idgames/idstuff/quake2/$QUAKE_EXE" "$QUAKE_EXE"
}

build () {
	unzip -q "$QUAKE_EXE" -d "quake_dir" || return 1

	# fix permissions
	chmod 0644 $(find "quake_dir/Install" -type f)
	chmod 0755 $(find "quake_dir/Install" -type d)
}

install () {
	mkdir -p "$DESTDIR$PREFIX/games/quake2"
	mkdir -p "$DESTDIR$PREFIX/doc/quake2"
	cp -p -r "quake_dir/Install/Data/baseq2" "$DESTDIR$PREFIX/games/quake2/"
	cp -p -r "quake_dir/Install/Data/DOCS"/* "$DESTDIR$PREFIX/doc/quake2/"
}
