#!/bin/sh

set -e
PACKAGES_DIR="$BUILDDIR/packages"

"$TINX" build "$@"

for PACKAGE in "$@" ; do
	PACKAGE_DIR="$PACKAGES_DIR/$PACKAGE"
	rm -fr "$PACKAGE_DIR"
	DESTDIR="$PACKAGE_DIR" "$TINX" --reinstall install "$PACKAGE"

	# make the tar
	(cd "$PACKAGE_DIR" && tar -cz * -f "../$PACKAGE.tar.gz")
done
