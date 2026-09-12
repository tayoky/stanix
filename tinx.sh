tinx_help () {
	echo "tinx [OPTIONS] ACTIONS [PACKAGES...]"
	echo "options :"
	echo "--build-package : select a build tool package"
	echo "--host-package  : select a host tool package"
	echo "actions :"
	echo "get-source : download and prepare the source for a package"
	echo "build      : build a package"
	echo "install    : install a package (by default to sysroot/build-env)"
}

tinx_error () {
	echo "tinx :" "$@" >&2
}

tinx_log () {
	echo "tinx :" "$@" >&2
}

tinx_setup_environ () {
	if test "$PACKAGE_TYPE" = "build" ; then
		export PREFIX="$BUILD_PREFIX"
		export TARGET="$HOST"
	elif test "$PACKAGE_TYPE" = "host" ; then
		export DESTDIR="${SYSROOT:-"$DESTDIR"}"
	fi
}

tinx_select_package () {
	configure () {
		true
	}
	build () {
		make -j"$PARALLELISM"
	}
	install () {
		make install DESTDIR="$DESTDIR"
	}
	tinx_setup_environ
	if test -f "$1.sh" ; then
		. "$1.sh"
	else
		tinx_error "unknown package $1"
		return 1
	fi
}

tinx_install_dependencies () {
	for DEP in $BUILD_DEPENDENCIES ; do
		"$TINX" --build-package install "$DEP" || return 1
	done
	for DEP in $DEPENDENCIES ; do
		"$TINX" --host-package install "$DEP" || return 1
	done
}

tinx_download () {
	if test "$#" != 2 ; then
		tinx_error "usage : tinx_download URL OUT"
		return 1
	fi
	URL="$1"
	OUT="$2"

	tinx_log "download $URL..."
	test "$DRY_RUN" = "yes" && return 0
	"$CURL" -L -o "$OUT" "$URL"
}

tinx_unpack () {
	if test "$#" != 2 ; then
		tinx_error "usage : tinx_unpack TAR OUT"
		return 1
	fi
	ARCHIVE="$1"
	OUT="$2"
	tinx_log "unpack $ARCHIVE..."
	test "$DRY_RUN" = "yes" && return 0
	
	# extract to tmp
	mkdir -p "$TMPDIR/$ARCHIVE" || return 1
	tar xf "$ARCHIVE" -C "$TMPDIR/$ARCHIVE" || return 1

	# move the child dir to main one
	mv -T "$TMPDIR/$ARCHIVE"/*/ "$OUT" || return 1
	rm -fr "$TMPDIR/$ARCHIVE" || return 1

	return 0
}

tinx_clone_commit () {
	if test "$#" != 3 ; then
		tinx_error "usage : tinx_clone_commit GIT COMMIT OUT"
		return 1
	fi
	tinx_log "clone $1#$2..."
	git clone --depth 1 "$1" "$3" || return 1
	git -C "$3" fetch --depth=1 origin "$2" || return 1
	git -C "$3" checkout --detach  "$2"
}

tinx_apply_patches () {
	if test -f "sources/$1/patches"/*.patch ; then
		for PATCH in "$TOP/sources/$1/patches"/*.patch ; do
			tinx_log "apply $PATCH..."
			patch -d "$SOURCE_DIR" -ruN -f -p1 -i "$PATCH" || return 1
		done
	fi
}

tinx_get_source () {
	prepare () {
		true
	}
	. "sources/$1/$1.sh"
	if test -n "$TAR" ; then
		mkdir -p "$BUILDDIR/tar"
		TAR_NAME="${TAR##*/}"
		TAR_FILE="$BUILDDIR/tar/$TAR_NAME"
		SOURCE_DIR="$BUILDDIR/tar/${TAR_NAME%%.tar.*}"
		if test -d "$SOURCE_DIR" ; then
			return 0
		fi
		if ! test -f "$TAR_FILE" ; then
			tinx_download "$TAR" "$TAR_FILE" || return 1
		fi
		tinx_unpack "$TAR_FILE" "$SOURCE_DIR" || return 1
	elif test -n "$GIT" ; then
		GIT_NAME="${GIT##*/}"
		SOURCE_DIR="$BUILDDIR/git/$GIT_NAME-$VERSION"
		if test -d "$SOURCE_DIR" ; then
			return 0
		fi
		tinx_clone_commit "$GIT" "$COMMIT" "$SOURCE_DIR" || return 1
	elif test -n "$DIR" ; then
		SOURCE_DIR="$DIR"
	else
		tinx_error "no TAR GIT or DIR specified for source $1"
	fi
	tinx_apply_patches "$1" || return 1
	prepare
}

tinx_configure () {
	tinx_install_dependencies || return 1
	test -z "$SOURCE" && return 0
	tinx_get_source "$SOURCE" || return 1
	BUILD_DIR="$BUILDDIR/$PACKAGE_TYPE-packages/$PACKAGE"
	if (! test -f "$BUILD_DIR/.tinx-configured") || test "$RECONFIGURE" = "yes" ; then
		tinx_log "configure $PACKAGE..."
		test "$DRY_RUN" = "yes" && return 0
		mkdir -p "$BUILD_DIR"
		(cd "$BUILD_DIR" && configure) || return 1
		touch "$BUILD_DIR/.tinx-configured"
	fi
}

tinx_build () {
	tinx_configure || return 1
	test -z "$SOURCE" && return 0
	if (! test -f "$BUILD_DIR/.tinx-built") || test "$REBUILD" = "yes" ; then
		tinx_log "build $PACKAGE..."
		test "$DRY_RUN" = "yes" && return 0
		(cd "$BUILD_DIR" && build) || return 1
		touch "$BUILD_DIR/.tinx-built"
	fi
}

tinx_install () {
	tinx_build || return 1
	test -z "$SOURCE" && return 0
	if (! test -f "$BUILD_DIR/.tinx-installed") || test "$REINSTALL" = "yes" ; then
		tinx_log "install $PACKAGE..."
		test "$DRY_RUN" = "yes" && return 0
		(cd "$BUILD_DIR" && install) || return 1
		touch "$BUILD_DIR/.tinx-installed"
	fi
}

TINX="$(realpath "$0")"
cd "$(dirname "$0")"

: ${BUILDDIR:="$PWD/build"}
: ${SYSROOT:="$BUILDDIR/sysroot"}
: ${BUILD_PREFIX:="$BUILDDIR/build-env"}
: ${GNU_MIRROR:="https://ftp.gnu.org"}
: ${STANIX_MIRROR:="https://github.com/tayoky"}
: ${CURL:="curl"}
: ${PARALLELISM:="$(nproc || echo 1)"}
: ${DRY_RUN:="no"}
: ${HOST:="$(uname -m)-stanix"}
: ${CFLAGS:="-Wall -Wextra -O2"}
: "${TMPDIR:=${TMP:=${TEMP:-/tmp}}}"

# add build tools to env
export PATH="$BUILD_PREFIX/bin:/$PATH"

export CONFIG_SUB="$PWD/config.sub"

export BUILDDIR SYSROOT BUILD_PREFIX CFLAGS CXXFLAGS="$CFLAGS"
export GNU_MIRROR CURL
export PARALLELISM DRY_RUN
export HOST
export TOP="$PWD"

RECONFIGURE="no"
REBUILD="no"-
REINSTALL="no"
PACKAGE_TYPE="host"

for I in "$@" ; do
	case "$I" in
		--reconfigure)
			RECONFIGURE=yes
			REBUILD=yes
			REINSTALL=yes
			;;
		--rebuild)
			REBUILD=yes
			REINSTALL=yes
			;;
		--reinstall)
			REINSTALL=yes
			;;
		--dry-run)
			DRY_RUN=yes
			;;
		--package-type=*)
			PACKAGE_TYPE="${I##*=}"
			;;
		--build-package)
			PACKAGE_TYPE="build"
			;;
		--host-package)
			PACKAGE_TYPE="host"
			;;
		--host=*)
			HOST="${I##*=}"
			;;
		--help)
			tinx_help
			exit 0
			;;
		--*)
			tinx_error "unkown option '$I'"
			exit 1
			;;
		*)
			break
			;;
	esac
	shift
done

if test -z "$1" ; then
	tinx_error "no action specified"
	exit 1
fi

ACTION="$1"
shift

if test -z "$1" ; then
	# by default build the base package
	set -- "stanix-base"
fi

for PACKAGE in "$@" ; do
	tinx_select_package "$PACKAGE_TYPE-packages/$PACKAGE" || exit 1

	case "$ACTION" in
		get-source)
			tinx_get_source "$SOURCE" || exit 1
			;;
		configure)
			tinx_configure || exit 1
			;;
		build)
			tinx_build || exit 1
			;;
		install)
			tinx_install || exit 1
			;;
		*)
			tinx_error "unknown action '$ACTION'"
			exit 1
			;;
	esac
done
