VERSION="1.8"
DEPENDENCIES="doomgeneric"

configure () {
	tinx_download "https://www.gamers.org/pub/idgames/idstuff/doom/doom-1.8.wad.gz" "doom-$VERSION.wad.gz"
}

build () {
	gunzip -kf "doom-$VERSION.wad.gz" || return 1
	chmod 0644 "doom-$VERSION.wad"
}

install () {
	mkdir -p "$DESTDIR$PREFIX/games/doom"
	cp -p "doom-$VERSION.wad" "$DESTDIR$PREFIX/games/doom/doom1.wad"
}
