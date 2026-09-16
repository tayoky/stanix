#!/bin/sh

# generate cross files
CROSSDIR="$DESTDIR$PREFIX/lib/cross-files"
mkdir -p "$CROSSDIR"

CC="$TARGET-gcc"
CXX="$TARGET-g++"
AR="$TARGET-ar"
LD="$TARGET-ld"
AS="$TARGET-as"
STRIP="$TARGET-strip"

# create meson cross file
# and yes we assume little endian
echo "# generated automatically by $(basename "$0")
[binaries]
c   = '$CC'
cpp = '$CXX'
ar  = '$AR'
ld  = '$LD'
as  = '$AS'
strip = '$STRIP'

[host_machine]
system     = '${TARGET#*-}'
cpu_family = '${TARGET%%-*}'
cpu        = '${TARGET%%-*}'
endian     = 'little'
[properties]
sys_root = '$SYSROOT'
" > "$CROSSDIR/$TARGET.meson"

# now cmake cross file
BASE_SYSTEM_NAME="${TARGET#*-}"
BASE_SYSTEM_NAME_FIRST="$(echo "$BASE_SYSTEM_NAME" | cut -c1)"
BASE_SYSTEM_NAME_REST="$(echo "$BASE_SYSTEM_NAME" | cut -c2-)"
SYSTEM_NAME="$(echo "$BASE_SYSTEM_NAME_FIRST" | tr a-z A-Z)$BASE_SYSTEM_NAME_REST"
echo "# generated automatically by $(basename "$0")
set(CMAKE_SYSTEM_NAME \"$SYSTEM_NAME\")

set(CMAKE_C_COMPILER   $CC)
set(CMAKE_CXX_COMPILER $CXX)
set(CMAKE_LINKER       $LD)

set(CMAKE_SYSROOT \"$SYSROOT\")

set(CMAKE_FIND_ROOT_PATH \"$SYSROOT\")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" > "$CROSSDIR/$TARGET.cmake"
