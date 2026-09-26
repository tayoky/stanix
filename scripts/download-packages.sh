#!/bin/sh

set -e

BASE_URL="https://github.com/tayoky/packages/releases/download/${REPO_RELEASE:-"stable"}"
DOWNLOAD_DIR="$BUILDDIR/downloads"
mkdir -p "$DOWNLOAD_DIR"
mkdir -p "$SYSROOT"

for PACKAGE in "$@" ; do
	TAR_NAME="$PACKAGE.tar.gz"
	echo "$BASE_URL/$TAR_NAME"
	curl -fL -o "$DOWNLOAD_DIR/$TAR_NAME" "$BASE_URL/$TAR_NAME"

	# unpack on sysroot
	tar -xf "$DOWNLOAD_DIR/$TAR_NAME" -C "$SYSROOT"
done
