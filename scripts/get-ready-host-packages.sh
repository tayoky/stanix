#!/bin/sh

set -e

READY_PACKAGES="$("$TINX" get-ready "$@")"
READY_HOST_PACKAGES=""
for PACKAGE_PATH in $READY_PACKAGES ; do
	case "$PACKAGE_PATH" in
		host-packages/*)
			READY_HOST_PACKAGES="$READY_HOST_PACKAGES ${PACKAGE_PATH##*-packages/}"
			;;
	esac
done

echo "$READY_HOST_PACKAGES"
