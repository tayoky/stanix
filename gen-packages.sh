#!/bin/sh

DEST="$PWD"
cd "$1"
TOP="$PWD"

: ${GNU_MIRROR:="https://ftp.gnu.org"}
: ${TAYOKY_MIRROR:="https://github.com/tayoky"}
: ${STANIX_MIRROR:="https://github.com/stanix-org"}

echo "---
title: Packages
---
A list of current Stanix packages. For an always up to date list see [https://github.com/tayoky/stanix/blob/main/host-packages](https://github.com/tayoky/stanix/blob/main/host-packages)" > "$DEST/packages/index.md"

for PACKAGE_PATH in host-packages/*.sh ; do
	VERSION="unknown"
	REVISION=""
	WEBSITE=""
	DEPENDENCIES=""
	SOURCE=""
	TAR=""
	TAR_HASH=""
	GIT=""
	COMMIT=""
	TAG=""
	DIR=""
	. "$PACKAGE_PATH"
	BASENAME="$(basename "$PACKAGE_PATH")"
	NAME="${BASENAME%%.sh}"
	test -n "$SOURCE" && . "sources/$SOURCE/$SOURCE.sh"
	echo "- [$NAME]($NAME)" >> "$DEST/packages/index.md"
	echo "generating packages/$NAME.md ..."
	{
		echo "---
title: $NAME
---

# Description
version : $VERSION  "
		test -n "$WEBSITE"  && echo "website : [$WEBSITE]($WEBSITE)  "
		test -n "$TAR"      && echo "tarball : [$TAR]($TAR)  "
		test -n "$TAR_HASH" && echo "hash    : $TAR_HASH  "
		test -n "$GIT"      && echo "git     : [$GIT]($GIT)  "
		test -n "$COMMIT"   && echo "commit  : $COMMIT  "
		test -n "$TAG"      && echo "tag     : $TAG  "
		test -n "$DIR"      && echo "dir     : \`\${STANIX_REPO}${DIR#$TOP}\`  "
	
		if test -n "$DEPENDENCIES" ; then
			echo "
# Dependencies"
			for i in $DEPENDENCIES ; do
				echo "- [$i]($i)"
			done
		fi

		echo "
# Install
To build and install this package, run  
\`\`\`sh
./tinx.sh install $NAME
\`\`\`

Alternatively you can try to download pre compiled of this package using
\`\`\`sh
./tinx.sh run scripts/download-packages.sh $NAME
\`\`\`"

		echo "
# Manifest
\`\`\`sh"
		cat "$PACKAGE_PATH"
		echo "\`\`\`"
	} > "$DEST/packages/$NAME.md"
done
