VERSION="2.16.03"
SOURCE="nasm"
WEBSITE="https://nasm.us"
DEPENDENCIES="stanix-base"

configure () {
	# thanks bananymous for the --disable-gdb
	./configure --host="$HOST" --prefix="$PREFIX" --disable-gdb
}
