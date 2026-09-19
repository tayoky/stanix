VERSION=2.69
SOURCE=autoconf

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX"
}
