# source this in your tmakegen

tmake_init () {
	MAKEFILE="$(realpath ./Makefile)"
	: ${DIR:="$(realpath "$(dirname "$0")")"}
	TARGETS=""
	: ${TMAKE:="$DIR/tmake.sh"}
	: ${SCRIPT:="$(realpath --relative-to="$DIR" "$0")"}
	: ${BUILDDIR:=$(realpath build)}
	SCRIPT="$(realpath --relative-to="$DIR" "$SCRIPT")"
	echo "# automatically generated from $(basename "$0")
# DO NOT EDIT

# tconf might have generated a config.mk
-include config.mk

# a few standard variables
VERSION ?= \$(shell git describe --tags --always 2>/dev/null || echo unknown)
NAME ?= $NAME
BUILDDIR ?= $(realpath --relative-to="$DIR" "$BUILDDIR")
PREFIX ?= /usr/local
STATIC ?= yes
SHARED ?= no
CFLAGS ?= -Wall -Wextra
CFLAGS += -DVERSION='\"\$(VERSION)\"'

ifeq (\$(HAVE_MMD) \$(HAVE_MP),yes yes)
	CFLAGS += -MMD -MP
endif

ifeq (\$(V),1)
	Q =
else
	Q = @
endif

.PHONY : all
all :

.PHONY : install
install :

.PHONY : uninstall
uninstall :" > "$MAKEFILE"
}

tmake_fini () {
	# add a few standard rules
	echo "
.PHONY : targets
targets :
	@echo \"====== $NAME targets ======\"
	@echo \"====== globals targets ======\"
	@echo \"all       : build every component\"
	@echo \"install   : install every component\"
	@echo \"uninstall : uninstall every component\"
	@echo \"clean     : clean every component\"
$(for TARG in $TARGETS ; do
	echo "	@echo \"====== $TARG targets ======\"
	@echo \"all-$TARG       : build $TARG\"
	@echo \"install-$TARG   : install $TARG\"
	@echo \"uninstall-$TARG : uninstall $TARG\"
	@echo \"clean-$TARG     : clean $TARG\""
done)

Makefile : $SCRIPT $(realpath --relative-to="$DIR" "$TMAKE")
	@echo \"GEN Makefile\"
	\$(Q)./$SCRIPT

.PHONY : clean
clean :
	@echo \"CLEAN \$(BUILDDIR)\"
	\$(Q)rm -fr \"\$(BUILDDIR)\""
} >> "$MAKEFILE"

tmake_error () {
	echo "$@" >&2
}

tmake_add_file () {
	case "$1" in
		*.c)
			HAVE_C=yes
			;;
		*.cxx|*.cpp|*.C)
			HAVE_CXX=yes
			;;
		*.s)
			HAVE_S=yes
			;;
		*)
			;;
	esac
}

tmake_is_flags () {
	case "$1" in
		*=*)
			return 0
			;;
		*)
			return 1
			;;
	esac
}

tmake_apply_flags () {
	case "$1" in
		CFLAGS=*)
			TARGET_CFLAGS="$TARGET_CFLAGS ${1#*=}"
			;;
		CXXFLAGS=*)
			TARGET_CXXFLAGS="$TARGET_CXXFLAGS ${1#*=}"
			;;
		ASFLAGS=*)
			TARGET_ASFLAGS="$TARGET_ASFLAGS ${1#*=}"
			;;
		LDFLAGS=*)
			TARGET_LDFLAGS="$TARGET_LDFLAGS ${1#*=}"
			;;
		DEPENDENCIES=*)
			TARGET_DEPENDENCIES="$TARGET_DEPENDENCIES ${1#*=}"
			;;
		DDEPENDENCIES=*)
			TARGET_DDEPENDENCIES="$TARGET_DDEPENDENCIES ${1#*=}"
			;;
		*)
			tmake_error "unknown flag '$1'"
			return 1
			;;
	esac
}

tmake_src_filter () {
	F=""
	for FILE in "$@" ; do
		tmake_is_flags "$FILE" && continue
		F="$F $FILE"
	done
	echo "$F"
}

tmake_add_compile_rules () {
	if test "$#" != 1 ; then
		tmake_error "usage : tmake_add_compile_rules TARGET"
		return 1
	fi
	if test "$HAVE_C" = "yes" ; then
		echo "
\$(BUILDDIR)/$1/%.c.o : %.c
	@mkdir -p \"\$(@D)\"
	@echo \"CC \$<\"
	\$(Q)\$(CC) $TARGET_CFLAGS -o \$@ -c \$<"
	fi
	if test "$HAVE_GEN_C" = "yes" ; then
		echo "
\$(BUILDDIR)/$1/%.c.o : \$(BUILDDIR)/$1/%.c
	@mkdir -p \"\$(@D)\"
	@echo \"CC \$<\"
	\$(Q)\$(CC) $TARGET_CFLAGS -o \$@ -c \$<"
	fi
	if test "$HAVE_CXX" = "yes" ; then
		echo "
\$(BUILDDIR)/$1/%.c.o : %.cxx
	@mkdir -p \"\$(@D)\"
	@echo \"CXX \$<\"
	\$(Q)\$(CXX) $TARGET_CXXFLAGS -o \$@ -c \$<"
	fi
	if test "$HAVE_S" = "yes" ; then
		echo "
\$(BUILDDIR)/$1/%.s.o : %.s
	@mkdir -p \"\$(@D)\"
	@echo \"AS \$<\"
	\$(Q)\$(AS) $TARGET_ASFLAGS -o \$@ \$<"
	fi
}

tmake_add_prefix () {
	PREF="$1"
	shift
	RES=""
	for I in "$@" ; do
		RES="$RES $PREF$I"
	done
	echo "$RES"
}

tmake_add_target () {
	if test -z "$2" ; then
		tmake_error "usage : tmake_add_target TARGET FILES PREFIX [FLAGS|SOURCE...]"
		return 1
	fi

	TARG="$1"
	FILES="$2"
	PREF="$3"
	: ${HAVE_C:="no"}
	: ${HAVE_GEN_C:="no"}
	: ${HAVE_CXX:="no"}
	: ${HAVE_S:="no"}
	TARGET_CFLAGS="\$(CFLAGS)"
	TARGET_CXXFLAGS="\$(CXXFLAGS)"
	TARGET_ASFLAGS="\$(ASFLAGS)"
	TARGET_LDFLAGS="\$(LDFLAGS)"
	TARGET_DEPENDENCIES=""
	TARGET_DDEPENDENCIES=""
	shift 3
	for SRC in "$@" ; do
		if tmake_is_flags "$SRC" ; then
			tmake_apply_flags "$SRC"
		else
			tmake_add_file "$SRC"
		fi
	done

	echo "
# ==== $TARG target ====
${VAR_SETUP:-"
ALL_$TARG = $FILES"}
SRC_$TARG =$(tmake_src_filter "$@")
OBJ_$TARG = \$(SRC_$TARG:%=\$(BUILDDIR)/$TARG/%.o)
DEPS_$TARG = \$(SRC_$TARG:%=\$(BUILDDIR)/$TARG/%.d)

.PHONY : all-$TARG
all : all-$TARG
all-$TARG : \$(ALL_$TARG)

# include dependencies files
-include \$(DEPS_$TARG)

.PHONY : install-$TARG
install : install-$TARG
install-$TARG : all-$TARG$(tmake_add_prefix "install-" $TARGET_DEPENDENCIES)
	@mkdir -p \"\$(DESTDIR)\$(PREFIX)/$PREF\"
	@echo \"INSTALL \$(ALL_$TARG)\"
	\$(Q)cp \$(ALL_$TARG) \"\$(DESTDIR)\$(PREFIX)/$PREF\"

.PHONY : uninstall-$TARG
uninstall : uninstall-$TARG
uninstall-$TARG :
	@echo \"UNINSTALL$(TO_REMOVE=""
	for FILE in $FILES ; do
		TO_REMOVE="$TO_REMOVE \$(DESTDIR)\$(PREFIX)/$PREF/${FILE#"\$(BUILDDIR)/$TARG/"}"
	done
	echo "$TO_REMOVE")\"
	\$(Q)rm -f$(TO_REMOVE=""
	for FILE in $FILES ; do
		TO_REMOVE="$TO_REMOVE \"\$(DESTDIR)\$(PREFIX)/$PREF/${FILE#"\$(BUILDDIR)/$TARG/"}\""
	done
	echo "$TO_REMOVE")

.PHONY : clean-$TARG
clean-$TARG :
	@echo \"CLEAN \$(BUILDDIR)/$TARG\"
	\$(Q)rm -fr \"\$(BUILDDIR)/$TARG\""


	tmake_add_compile_rules "$TARG"

	TARGETS="$TARGETS $TARG"
	ALL_DEPENDENCIES="\$(OBJ_$TARG)$TARGET_DDEPENDENCIES"
	for DEPENDENCY in $TARGET_DEPENDENCIES ; do
		ALL_DEPENDENCIES="$ALL_DEPENDENCIES \$(ALL_$DEPENDENCY)"
	done
	HAVE_C=""
	HAVE_GEN_C=""
	HAVE_CXX=""
	HAVE_S=""
} >> "$MAKEFILE"

tmake_add_executable () {
	TARG="$1"
	EXE="\$(BUILDDIR)/$TARG/$TARG"
	VAR_SETUP=""
	shift
	tmake_add_target "$TARG" "$EXE" "bin" "$@"
	echo "
$EXE : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"CCLD $TARG\"
	\$(Q)\$(CC) $TARGET_CFLAGS $TARGET_LDFLAGS -o \$@ \$^"
} >> "$MAKEFILE"

tmake_add_shared_library () {
	TARG="$1"
	LIB="\$(BUILDDIR)/$TARG/$TARG.so"
	VAR_SETUP=""
	shift
	tmake_add_target "$TARG" "$LIB" "lib" "CFLAGS=-fPIC" "$@"
	echo "
$LIB : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"CCLD $TARG.so\"
	\$(Q)\$(CC) -shared $TARGET_CFLAGS $TARGET_LDFLAGS -o \$@ \$^"
} >> "$MAKEFILE"

tmake_add_static_library () {
	TARG="$1"
	LIB="\$(BUILDDIR)/$TARG/$TARG.a"
	VAR_SETUP=""
	shift
	tmake_add_target "$TARG" "$LIB" "lib" "$@"
	echo "
$LIB : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"AR $TARG.a\"
	\$(Q)\$(AR) rcs $TARGET_ARFLAGS \$@ \$^"
} >> "$MAKEFILE"

tmake_add_library () {
	TARG="$1"
	LIBA="\$(BUILDDIR)/$TARG/$TARG.a"
	LIBSO="\$(BUILDDIR)/$TARG/$TARG.so"
	LIBS="$LIBA $LIBSO"
	VAR_SETUP="
ALL_$TARG =
LINK_$TARG =
ifeq (\$(STATIC),yes)
ALL_$TARG += $LIBA
LINK_$TARG = $LIBA
endif
ifeq (\$(SHARED),yes)
ALL_$TARG += $LIBSO
LINK_$TARG = $LIBSO
endif"
	shift
	tmake_add_target "$TARG" "$LIBS" "lib" "$@"
	echo "

ifeq (\$(STATIC),yes)
$LIBA : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"AR $TARG.a\"
	\$(Q)\$(AR) rcs $TARGET_ARFLAGS \$@ \$^
endif

ifeq (\$(SHARED),yes)
$LIBSO : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"CCLD $TARG.so\"
	\$(Q)\$(CC) -shared $TARGET_CRFLAGS $TARGET_LDFLAGS -o \$@ \$^
endif"
} >> "$MAKEFILE"

# replace this with add_data
tmake_add_data () {
	TARG="$1"
	DEST="$2"
	shift 2
	echo "
# ==== $TARG target ====
SRC_$TARG = $@
FILES_$TARG =$(F=""
	for I in "$@" ; do
		F="$F $(basename "$I")"
	done
	echo "$F")
DEST_$TARG = \$(FILES_$TARG:%=\$(DESTDIR)\$(PREFIX)/$DEST/%)

.PHONY : install-$TARG
install : install-$TARG
install-$TARG :
	@mkdir -p \"\$(DESTDIR)\$(PREFIX)/$DEST\"
	@echo \"INSTALL_DATA \$(SRC_$TARG)\"
	\$(Q)cp -r \$(SRC_$TARG) \"\$(DESTDIR)\$(PREFIX)/$DEST\"
.PHONY : uninstall-$TARG
uninstall : uninstall-$TARG
uninstall-$TARG :
	@echo \"UNINSTALL \$(DEST_$TARG)\"
	\$(Q)rm -fr \$(DEST_$TARG)"
} >> "$MAKEFILE"
