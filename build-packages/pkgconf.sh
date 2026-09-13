VERSION="2.3.0"
SOURCE="pkgconf"
BUILD_DEPENDENCIES="autoconf automake libtool"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}

install () {
	make install DESTDIR="$DESTDIR"
	BINDIR="$DESTDIR$PREFIX/bin"
	ln -sf pkgconf "$BINDIR/pkg-config"
	ln -sf pkgconf "$BINDIR/$TARGET-pkgconf"
	ln -sf pkgconf "$BINDIR/$TARGET-pkg-config"

	# build the personality
	mkdir -p "$DESTDIR$PREFIX/share/pkgconfig/personality.d"
	echo "Triplet: $TARGET
SysrootDir: $SYSROOT
DefaultSearchPaths: $SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig: $SYSROOT/usr/local/lib/pkgconfig:$SYSROOT/usr/local/share/pkgconfig
SystemIncludePaths: $SYSROOT/usr/include:$SYSROOT/usr/local/include
SystemLibraryPaths: $SYSROOT/usr/lib:$SYSROOT/usr/local/lib" > "$DESTDIR$PREFIX/share/pkgconfig/personality.d/$TARGET.personality"
}
