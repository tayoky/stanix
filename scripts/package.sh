#!/bin/sh

set -e
PACKAGES_DIR="$BUILDDIR/packages"

# first,install every package to sysroot
"$TINX" install "$@"

for PACKAGE in "$@" ; do
	PACKAGE_DIR="$PACKAGES_DIR/$PACKAGE"
	rm -fr "$PACKAGE_DIR"
	
	mkdir -p "$PACKAGE_DIR"

	DESTDIR="$PACKAGE_DIR" "$TINX" --no-deps --reinstall install "$PACKAGE"

	# make the tar

	if test -n "$(ls "$PACKAGE_DIR"/*)" ; then
		(cd "$PACKAGE_DIR" && tar -cz * -f "../$PACKAGE.tar.gz")
	else
		tar -cz -f "$PACKAGES_DIR/$PACKAGE.tar.gz" --files-from=/dev/null
	fi
done
