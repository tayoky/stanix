# source this script in your configure

tconf_print () {
	echo "$@" 1>&2
}

tconf_set_var () {
	eval "$1='$2'"
}

tconf_get_var () {
	eval "echo \"\$$1\""
}

tconf_to_macro_name () {
	echo "$@" | tr "a-z./ -" "A-Z____"
}

tconf_to_file_name () {
	echo "$@" | tr " " "_"
}

tconf_help () {
	echo "usage : $(basename "$0") [OPTIONS...]
generate config.mk from environement
--cc=CC set the C compiler
--cxx=CXX set the C++ compiler
--as=AS set the assembler
--ar=AR set the archiver
--ld=LD set the linker
--objcopy=OBJCOPY set the object copy utility
--strip=STRIP set the striper
--nm=NM set the nm
--pkgconfig=PKGCONFIG set the pkg-config
--cflags=CFLAGS set cutsom flags for C compilation
--cxxflags=CXXFLAGS set custom flags for C++ compilation
--arflags=ASFLAGS set the flags for assembling
--asflags=ARFLAGS set the flags for archiving
--ldflags=LDFLAGS set the flags for linking
--host=HOST set the host os, can be used for cross compiling
--build=BUILD set the build os, can be set if tconf cannot determinate the build os
--clear-cache clear the cache before doing anything
--prefix=PREFIX set the prefix
--sysroot=SYSROOT, --with-sysroot=SYSROOT set the sysroot
--debug compile with debug options actived
--enable-XXX compile with a specific feature enabled
--disable-XXX compile with a specific feature disabled
--help show this help and exit"
}

tconf_init () {
	TOP="$(realpath "$(dirname $0)")"
	OPT=""
	if test -z "$TCONF_DIR" ; then
		# export so subdirs can use the same tconf dir
		export TCONF_DIR="$TOP/tconf"
		mkdir -p "$TCONF_DIR"
	fi

	# defaults
	test -z "$PREFIX" && PREFIX="/usr/local"
	test -z "$CFLAGS" && CFLAGS="-Wall -Wextra"
	test -z "$DEBUG" && DEBUG="no"

	# parse options
	for i in "$@" ; do
		case "$i" in
		--cc=*|CC=*)
			CC="${i#*=}"
			;;
		--cxx=*|CXX=*)
			CXX="${i#*=}"
			;;
		--as=*|AS=*)
			AS="${i#*=}"
			;;
		--ar=*|AR=*)
			AR="${i#*=}"
			;;
		--ld=*|LD=*)
			LD="${i#*=}"
			;;
		--objcopy=*|OBJCOPY=*)
			OBJCOPY="${i#*=}"
			;;
		--readelf=*|READELF=*)
			READELF="${i#*=}"
			;;
		--strip=*|STRIP=*)
			STRIP="${i#*=}"
			;;
		--nm=*|NM=*)
			NM="${i#*=}"
			;;
		--pkgconfig=*|PKGCONFIG=*)
			PKGCONFIG="${i#*=}"
			;;
		--cflags=*|CFLAGS=*)
			CFLAGS="${i#*=}"
			;;
		--cxxflags=*|CXXFLAGS=*)
			CXXFLAGS="${i#*=}"
			;;
		--asflags=*|ASFLAGS=*)
			ASFLAGS="${i#*=}"
			;;
		--arflags=*|ARFLAGS=*)
			ARFLAGS="${i#*=}"
			;;
		--ldflags=*|LDFLAGS=*)
			LDFLAGS="${i#*=}"
			;;
		--host=*|HOST=*)
			HOST="${i#*=}"
			;;
		--build=*|BUILD=*)
			BUILD="${i#*=}"
			;;
		--prefix=*|PREFIX=*)
			PREFIX="${i#*=}"
			;;
		--with-sysroot=*|--sysroot=*)
			SYSROOT="${i#*=}"
			;;
		--debug)
			DEBUG=yes
			;;
		--enable-*)
			tconf_set_var $(tconf_to_macro_name "${i#--*-}") "yes"
			;;
		--disable-*)
			tconf_set_var $(tconf_to_macro_name "${i#--*-}") "no"
			;;
		--clear-cache)
			rm -fr "$TCONF_DIR/"*
			;;
		--help)
			tconf_help
			exit 0
			;;
		--*)
			tconf_print "unknow option '$i' (see --help)"
			exit 1
			;;
		esac
	done

	# make path absolute
	test -n "$PREFIX" && PREFIX="$(realpath -m "$PREFIX")"
	test -n "$SYSROOT" && SYSROOT="$(realpath -m "$SYSROOT")"
	
	return 0
}

tconf_echo_conf () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_echo_conf NAME VAR"
		return 1
	fi
	test -n "$2" && echo "$1=$2"
}

tconf_fini () {
	test "$DEBUG" = "yes" && OPT="$OPT -g -DDEBUG=1"
	{
		echo "# automaticly generated from $(basename "$0")"
		tconf_echo_conf PREFIX "$PREFIX"
		tconf_echo_conf SYSROOT "$SYSROOT"
		tconf_echo_conf CC "$CC"
		tconf_echo_conf CXX "$CXX"
		tconf_echo_conf AS "$AS"
		tconf_echo_conf AR "$AR"
		tconf_echo_conf LD "$LD"
		tconf_echo_conf READELF "$READELF"
		tconf_echo_conf OBJCOPY "$OBJCOPY"
		tconf_echo_conf STRIP "$STRIP"
		tconf_echo_conf NM "$NM"
		tconf_echo_conf PKGCONFIG "$PKGCONFIG"

		# avoid triggering CFLAGS or CXXFLAGS just because of options
		# use them only if the corresponding compiler is used
		test -n "$CC"  && tconf_echo_conf CFLAGS "$CFLAGS$OPT"
		test -n "$CXX" && tconf_echo_conf CXXFLAGS "$CXXFLAGS$OPT"
		tconf_echo_conf ASFLAGS "$ASFLAGS"
		tconf_echo_conf ARFLAGS "$ASFLAGS"
		tconf_echo_conf LDFLAGS "$LDFLAGS"
		tconf_echo_conf HOST "$HOST"
		tconf_echo_conf ARCH "$ARCH"
		tconf_echo_conf DEBUG "$DEBUG"
	} > "$TOP/config.mk"
	return 0
}

tconf_add_subdir () {
	if test -z "$1" ; then
		tconf_print "usage : tconf_add_subdir SUBDIR [OPTIONS...]"
		return 1
	fi
	export CC CXX AS AR LD NM
	export READELF OBJCOPY STRIP PKGCONFIG
	export CFLAGS CXXFLAGS ASFLAGS
	export ARFLAGS LDFLAGS OPT
	export HOST BUILD TARGET
	export PREFIX SYSROOT DEBUG
	SUBDIR="$1"
	shift
	tconf_print "entering subdir $SUBDIR"
	(cd "$SUBDIR" && ./configure "$@")
	CODE=$?
	tconf_print "exiting subdir $SUBDIR"
	test "$CODE" != 0 && exit $CODE
	return 0
}

tconf_require () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_require NAME VAR"
		return 1
	fi
	if test -z "$2" ; then
		tconf_print "no $1 found"
		exit 1
	fi
}

tconf_check_code () {
	if test -z "$3" ; then
		tconf_print "usage : tconf_check_code CC NAME CODE [CFLAGS]"
		return 1
	fi

	FILE="$TCONF_DIR/check-$(tconf_to_file_name $2).c"
	mkdir -p "$(dirname "$FILE")"

	tconf_print -n "check $2... "
	
	# check if we aready checked this
	if test -f "$FILE.out" ; then
		tconf_print "yes(cached)"
		return 0
	fi

	echo "$3" > "$FILE"
	if env "CFLAGS=$CFLAGS $4" $1 "$FILE" -o "$FILE.out" >/dev/null 2>/dev/null ; then
		tconf_print "yes"
		return 0
	else
		tconf_print "no"
		return 1
	fi
}

tconf_check_code_define () {
	if test -z "$3" ; then
		tconf_print "usage : tconf_check_code_define CC NAME CODE [CFLAGS]"
		return 1
	fi

	if tconf_check_code "$1" "$2" "$3" "$4" ; then
		OPT="$OPT -DHAVE_$(tconf_to_macro_name "$2")=1"
	fi
}

tconf_check_func () {
	if [ $# != 3 ] ; then
		tconf_print "usage : tconf_check_func CC HEADER FUNC"
		return 1
	fi
	tconf_check_code_define "$1" "$3" "#include <$2>
void *volatile ptr = (void*)$3;
int main() {
	return 0;
}"
}

tconf_check_header () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_check_header CC HEADER"
		return 1
	fi

	tconf_check_code_define $1 "$2" "#include <$2>
int main () {
	return 0;
}"
}

tconf_require_header () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_require_header CC HEADER"
		return 1
	fi
	if ! tconf_check_header "$1" "$2" ; then
		tconf_print "$2 is required"
		exit 1
	fi
}

tconf_check_library () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_check_library CC LIBRARY"
		return 1
	fi
	tconf_check_code_define "$1" "lib$2" "int main() { return 0; }" "-l$2"
}

tconf_check_attribute () {
	if [ $# != 2 ] ; then
		tconf_print "usage : tconf_check_attribute CC ATTRIBUTE"
		return 1
	fi
	tconf_check_code_define "$1" "attribute $2" "int __attribute__(($2)) var; int main() { return var; }"
}

tconf_check_cc_option () {
	if [ "$#" != 2 ] ; then
		tconf_print "usage : tconf_check_cc_option CC OPTION"
		return 1
	fi
	tconf_check_code "$1" "$2" "int main(){return 0;}" "$2"
}

tconf_search_util () {
	if test -z "$2" ; then
		tconf_print "usage : tconf_search_util VAR NAME PREFIX UTILS..."
		return 1
	fi

	UTIL_VAR="$1"
	UTIL_NAME="$2"
	tconf_print -n "search $UTIL_NAME... "

	# test if aready set
	if test -n "$UTIL_VAR" && test -n "$(tconf_get_var $UTIL_VAR)" ; then
		tconf_print "$(tconf_get_var $UTIL_VAR)"
		return 0
	fi
	
	UTIL_PREFIX="$3"
	test -n "$UTIL_PREFIX" && UTIL_PREFIX="$UTIL_PREFIX-"

	# skip name var and prefix
	shift 3
	for util in "$@" ; do
		if ${UTIL_PREFIX}$util --version 2>/dev/null >/dev/null ; then
			test -n "$UTIL_VAR" && tconf_set_var $UTIL_VAR "${UTIL_PREFIX}$util"
			tconf_print "${UTIL_PREFIX}$util"
			return 0
		fi
	done
	tconf_print "no"
	tconf_print "no $UTIL_NAME found"
	exit 1
}

tconf_search_cc () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_cc PREFIX"
		return 1
	fi
	tconf_search_util CC "C compiler" "$1" gcc clang tcc cc
}

tconf_search_cxx () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_cxx PREFIX"
		return 1
	fi
	tconf_search_util CXX "C++ compiler" "$1" g++ clang++ c++ gpp cxx
}

tconf_search_as () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_as PREFIX"
		return 1
	fi
	tconf_search_util AS "assembler" "$1" gas as
}

tconf_search_ar () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_ar PREFIX"
		return 1
	fi
	tconf_search_util AR "archiver" "$1" ar llvm-ar "tcc -ar"
}

tconf_search_ld () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_ld PREFIX"
		return 1
	fi
	tconf_search_util LD "linker" "$1" ld
}

tconf_search_objcopy () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_objcopy PREFIX"
		return 1
	fi
	tconf_search_util OBJCOPY "objcopy" "$1" objcopy
}

tconf_search_strip () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_strip PREFIX"
		return 1
	fi
	tconf_search_util STRIP "strip" "$1" strip
}

tconf_search_nm () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_nm PREFIX"
		return 1
	fi
	tconf_search_util NM "nm" "$1" nm llvm-nm
}

tconf_search_pkgconfig () {
	if [ $# != 1 ] ; then
		tconf_print "usage : tconf_search_pkgconfig PREFIX"
		return 1
	fi
	tconf_search_util PKGCONFIG "pkg-config" "$1" pkg-config
}

tconf_find_build () {
	tconf_print -n "build os... "
	if test -n "$BUILD" ; then
		tconf_print "$BUILD"
		return 0
	fi
	if test -n "$CC_FOR_BUILD" && BUILD="$($CC_FOR_BUILD -dumpmachine )" ; then
		tconf_print "$BUILD"
		return 0
	fi
	tconf_print "unknow"
	return 1
}

tconf_find_host () {
	tconf_print -n "host os... "
	if test -n "$HOST" ; then
		tconf_print "$HOST"
		return 0
	fi
	if test -n "$CC" && HOST="$($CC -dumpmachine)" ; then
		tconf_print "$HOST"
		return 0
	fi
	tconf_print "unknow"
	return 1
}

tconf_find_os () {
	tconf_find_build
	tconf_find_host
}
