#!/bin/sh

set -e
PACKAGES_DIR="$BUILDDIR/packages"

"$TINX" build "$@"

for PACKAGE in "$@" ; do
	PACKAGE_DIR="$PACKAGES_DIR/$PACKAGE"
	rm -fr "$PACKAGE_DIR"
	
	mkdir -p "$PACKAGE_DIR/usr/share/tapm/"
	echo "# TODO : put stuff" > "$PACKAGE_DIR/usr/share/tapm/$PACKAGE.ini"
	DESTDIR="$PACKAGE_DIR" "$TINX" --no-deps --reinstall install "$PACKAGE"

	# make the tar
	(cd "$PACKAGE_DIR" && tar -cz * -f "../$PACKAGE.tar.gz")
done
