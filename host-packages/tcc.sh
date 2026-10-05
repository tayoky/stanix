SOURCE="tcc"
DEPENDENCIES="stanix-base"
WEBSITE="https://bellard.org/tcc/"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX" --sysroot="$SYSROOT" \
		--cpu="${HOST%%-*}" \
        --targetos="${HOST##*-}" \
		--enable-cross \
		--cross-prefix=$HOST- \
        --triplet="$HOST" \
        --sysincludepaths="/usr/include:/usr/local/include:$PREFIX/lib/tcc/include" \
        --libpaths="/lib:/usr/lib:/usr/local/lib:$PREFIX/lib/tcc" \
        --crtprefix="/usr/lib" \
        --elfinterp="/usr/lib/ld-tlibc.so" \
        --cc="gcc" --ar="$HOST-ar"

    # disable semaphores
	echo '#define CONFIG_TCC_SEMLOCK 0' >> config.h
}

build () {
    # TOP causes issues with tcc's makefile
	(unset TOP && make cross-${HOST%%-*} XTCC="$HOST-gcc" XAR="$HOST-ar")
}

install () {
    # TOP causes issues with tcc's makefile
	(unset TOP && make install DESTDIR="$DESTDIR" XTCC="$HOST-gcc" XAR="$HOST-ar") || return 1
    ln -sf "${HOST%%-*}-tcc" "$DESTDIR$PREFIX/bin/tcc"
}
