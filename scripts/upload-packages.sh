#!/bin/sh

set -e
"$TOP/scripts/package.sh" "$@"
PACKAGES_DIR="$BUILDDIR/packages"

for PACKAGE in "$@" ; do
	gh upload "${UPLOAD_RELEASE:-"nightly"}" --repo "tayoky/packages" --clobber "$PACKAGES_DIR/$PACKAGE.tar.gz"
done
