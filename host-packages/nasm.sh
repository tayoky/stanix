VERSION="2.16.03"
SOURCE="nasm"
WEBSITE="https://nasm.us"
DEPENDENCIES="stanix-base"
BUILD_DEPENDENCIES="tool-stanix-toolchain"

configure () {
	# thanks bananymous for the --disable-gdb
	"$SOURCE_DIR/configure" --host="$HOST" --prefix="$PREFIX" --disable-gdb
}
