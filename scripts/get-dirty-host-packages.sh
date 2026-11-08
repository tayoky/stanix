#!/bin/sh

set -e

DIRTY_PACKAGES="$("$TINX" get-dirty "$@")"
DIRTY_HOST_PACKAGES=""
for PACKAGE in $DIRTY_PACKAGES ; do
	case "$PACKAGE" in
		tool-*)
			# ignore tools
			;;
		*)
			DIRTY_HOST_PACKAGES="$DIRTY_HOST_PACKAGES $PACKAGE"
			;;
	esac
done

echo $DIRTY_HOST_PACKAGES
