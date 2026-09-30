VERSION="2.3.0"
SOURCE="pkgconf"
BUILD_DEPENDENCIES="autoconf automake libtool"
. "$TOP/scripts/gnu-package.sh"

configure () {
	gnu_configure --enable-shared
}

install () {
	make install DESTDIR="$DESTDIR"
	BINDIR="$DESTDIR$PREFIX/bin"
	ln -sf pkgconf "$BINDIR/pkg-config"
	ln -sf pkgconf "$BINDIR/$TARGET-pkgconf"
	ln -sf pkgconf "$BINDIR/$TARGET-pkg-config"

	# build the personality
	mkdir -p "$DESTDIR$PREFIX/share/pkgconfig/personality.d"
	echo "Triplet: $HOST
DefaultSearchPaths: /usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig
SystemIncludePaths: /usr/include:/usr/local/include
SystemLibraryPaths: /usr/lib:/usr/local/lib" > "$DESTDIR$PREFIX/share/pkgconfig/personality.d/$TARGET.personality"
}
