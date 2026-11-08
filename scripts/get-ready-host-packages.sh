#!/bin/sh

set -e

READY_PACKAGES="$("$TINX" get-ready "$@")"
READY_HOST_PACKAGES=""
for PACKAGE in $READY_PACKAGES ; do
	case "$PACKAGE" in
		tool-*)
			# ignore tools
			;;
		*)
			READY_HOST_PACKAGES="$READY_HOST_PACKAGES $PACKAGE"
			;;
	esac
done

echo $READY_HOST_PACKAGES
