BUILD_DEPENDENCIES="pkgconf"

configure () {
	true
}

build () {
	true
}

install () {
	export TARGET
	"$TOP/scripts/generate-cross-files.sh"
}
