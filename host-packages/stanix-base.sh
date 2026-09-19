SOURCE="stanix-base"
DEPENDENCIES="stanix-kernel tlibc tash tutils tvi stanix-modules stanix-libraries stanix-userspace limine-files libgcc_s"
. "$TOP/scripts/stanix-package.sh"

configure () {
	true
}

build () {
	true
}

install () {
	# install base sysroot
	mkdir -p "$DESTDIR"
	cp -Pf -rp "$SOURCE_DIR"/* "$DESTDIR/"

	# install os release
	"$TOP/scripts/generate-os-release.sh" > "$DESTDIR/etc/os-release"
}
