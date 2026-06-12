# source this in your tmakegen

TMAKE_VERSION="v0.1.8"

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
MAKEFLAGS += --no-builtin-rules

# tconf might have generated a config.mk
-include config.mk

# a few standard variables
VERSION := \$(shell git describe --tags --always 2>/dev/null || echo unknown)
NAME ?= $NAME
BUILDDIR ?= $(realpath -m --relative-to="$DIR" "$BUILDDIR")
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
uninstall :

.PHONY : clean
clean :" > "$MAKEFILE"
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
	@echo \"distclean : clean every component and delete build dir\"
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

.PHONY : distclean
distclean :
	@echo \"CLEAN \$(BUILDDIR)\"
	\$(Q)rm -fr \"\$(BUILDDIR)\""

	echo "generated $MAKEFILE" >&2
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
			TARGET_DDEPENDENCIES="$TARGET_DEPENDENCIES ${1#*=}"
			;;
		HAVE_C=*)
			HAVE_C="${1#*=}"
			;;
		HAVE_GEN_C=*)
			HAVE_GEN_C="${1#*=}"
			;;
		HAVE_CXX=*)
			HAVE_CXX="${1#*=}"
			;;
		HAVE_S=*)
			HAVE_S="${1#*=}"
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

	TARGET_TARGET="$1"
	FILES="$2"
	PREF="$3"
	HAVE_C="no"
	HAVE_GEN_C="no"
	HAVE_CXX="no"
	HAVE_S="no"
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
# ==== $TARGET_TARGET target ====
${VAR_SETUP:-"
ALL_$TARGET_TARGET = $FILES"}
SRC_$TARGET_TARGET =$(tmake_src_filter "$@")
OBJ_$TARGET_TARGET = \$(SRC_$TARGET_TARGET:%=\$(BUILDDIR)/$TARGET_TARGET/%.o)
DEPS_$TARGET_TARGET = \$(SRC_$TARGET_TARGET:%=\$(BUILDDIR)/$TARGET_TARGET/%.d)

.PHONY : all-$TARGET_TARGET
all : all-$TARGET_TARGET
all-$TARGET_TARGET : \$(ALL_$TARGET_TARGET)

# include dependencies files
-include \$(DEPS_$TARGET_TARGET)

.PHONY : install-$TARGET_TARGET
install : install-$TARGET_TARGET
install-$TARGET_TARGET : all-$TARGET_TARGET$(tmake_add_prefix "install-" $TARGET_DEPENDENCIES)
	@mkdir -p \"\$(DESTDIR)\$(PREFIX)/$PREF\"
	@echo \"INSTALL \$(ALL_$TARGET_TARGET)\"
	\$(Q)cp \$(ALL_$TARGET_TARGET) \"\$(DESTDIR)\$(PREFIX)/$PREF\"

.PHONY : uninstall-$TARGET_TARGET
uninstall : uninstall-$TARGET_TARGET
uninstall-$TARGET_TARGET :
	@echo \"UNINSTALL$(TO_REMOVE=""
	for FILE in $FILES ; do
		TO_REMOVE="$TO_REMOVE \$(DESTDIR)\$(PREFIX)/$PREF/${FILE#"\$(BUILDDIR)/$TARGET_TARGET/"}"
	done
	echo "$TO_REMOVE")\"
	\$(Q)rm -f$(TO_REMOVE=""
	for FILE in $FILES ; do
		TO_REMOVE="$TO_REMOVE \"\$(DESTDIR)\$(PREFIX)/$PREF/${FILE#"\$(BUILDDIR)/$TARGET_TARGET/"}\""
	done
	echo "$TO_REMOVE")

.PHONY : clean-$TARGET_TARGET
clean : clean-$TARGET_TARGET
clean-$TARGET_TARGET :
	@echo \"CLEAN \$(BUILDDIR)/$TARGET_TARGET\"
	\$(Q)rm -fr \"\$(BUILDDIR)/$TARGET_TARGET\""


	tmake_add_compile_rules "$TARGET_TARGET"

	TARGETS="$TARGETS $TARGET_TARGET"
	ALL_DEPENDENCIES="$TARGET_DDEPENDENCIES"
	for DEPENDENCY in $TARGET_DEPENDENCIES ; do
		ALL_DEPENDENCIES="$ALL_DEPENDENCIES\$(LINK_$DEPENDENCY) "
	done
	ALL_DEPENDENCIES="$ALL_DEPENDENCIES\$(OBJ_$TARGET_TARGET)"
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

tmake_add_static_library () {
	TARG="$1"
	LIB_TARG="static-$TARG"
	LIB="\$(BUILDDIR)/$LIB_TARG/$TARG.a"
	VAR_SETUP="
ALL_$LIB_TARG =
LINK_$LIB_TARG =
ifeq (\$(STATIC),yes)
ALL_$LIB_TARG += $LIB
LINK_$LIB_TARG = $LIB
ALL_$TARG += $LIB
LINK_$TARG = $LIB"
	shift
	tmake_add_target "$LIB_TARG" "$LIB" "lib" "$@"
	echo "
$LIB : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"AR $TARG.a\"
	\$(Q)\$(AR) rcs $TARGET_ARFLAGS \$@ \$(OBJ_$LIB_TARG)


.PHONY : all-$TARG
all-$TARG : all-$LIB_TARG

.PHONY : install-$TARG
install-$TARG : install-$LIB_TARG

.PHONY : uninstall-$TARG
uninstall-$TARG : uninstall-$LIB_TARG

.PHONY : clean-$TARG
clean-$TARG : clean-$LIB_TARG
endif"
} >> "$MAKEFILE"

tmake_add_shared_library () {
	TARG="$1"
	LIB_TARG="shared-$TARG"
	LIB="\$(BUILDDIR)/$LIB_TARG/$TARG.so"
	VAR_SETUP="
ALL_$LIB_TARG =
LINK_$LIB_TARG =
ifeq (\$(SHARED),yes)
ALL_$LIB_TARG += $LIB
LINK_$LIB_TARG = $LIB
ALL_$TARG += $LIB
LINK_$TARG = $LIB"
	shift
	tmake_add_target "$LIB_TARG" "$LIB" "lib" "CFLAGS=-fPIC" "$@"
	echo "
$LIB : $ALL_DEPENDENCIES
	@mkdir -p \"\$(@D)\"
	@echo \"CCLD $TARG.so\"
	\$(Q)\$(CC) -shared $TARGET_CFLAGS $TARGET_LDFLAGS -o \$@ \$^

.PHONY : all-$TARG
all-$TARG : all-$LIB_TARG

.PHONY : install-$TARG
install-$TARG : install-$LIB_TARG

.PHONY : uninstall-$TARG
uninstall-$TARG : uninstall-$LIB_TARG

.PHONY : clean-$TARG
clean-$TARG : clean-$LIB_TARG
endif"
} >> "$MAKEFILE"

tmake_add_library () {
	TARG="$1"
	shift
	
	echo "
# ==== $TARG target ====
ALL_$TARG  =
LINK_$TARG ="
	tmake_add_static_library "$TARG" "$@"
	tmake_add_shared_library "$TARG" "$@"
} >> "$MAKEFILE"

tmake_add_data () {
	TARG="$1"
	DEST="$2"
	shift 2
	echo "
# ==== $TARG target ====
SRC_$TARG = $@
FILES_$TARG = \$(shell for I in \$(SRC_$TARG) ; do basename \"\$\$I\"; done)
DEST_$TARG = \$(FILES_$TARG:%=\$(DESTDIR)\$(PREFIX)/$DEST/%)

.PHONY : install-$TARG
install : install-$TARG
install-$TARG : \$(SRC_$TARG)
	@mkdir -p \"\$(DESTDIR)\$(PREFIX)/$DEST\"
	@echo \"INSTALL_DATA \$(SRC_$TARG)\"
	\$(Q)cp -r \$(SRC_$TARG) \"\$(DESTDIR)\$(PREFIX)/$DEST\"

.PHONY : uninstall-$TARG
uninstall : uninstall-$TARG
uninstall-$TARG :
	@echo \"UNINSTALL \$(DEST_$TARG)\"
	\$(Q)rm -fr \$(DEST_$TARG)"
} >> "$MAKEFILE"
