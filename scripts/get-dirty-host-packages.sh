#!/bin/sh

set -e

DIRTY_PACKAGES="$("$TINX" get-dirty "$@")"
DIRTY_HOST_PACKAGES=""
for PACKAGE_PATH in $DIRTY_PACKAGES ; do
	case "$PACKAGE_PATH" in
		host-packages/*)
			DIRTY_HOST_PACKAGES="$DIRTY_HOST_PACKAGES ${PACKAGE_PATH##*-packages/}"
			;;
	esac
done

echo "$DIRTY_HOST_PACKAGES"
