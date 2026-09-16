SOURCE="tcc"
DEPENDENCIES="stanix-base"
WEBSITE="https://bellard.org/tcc/"

configure () {
	"$SOURCE_DIR/configure" --prefix="$PREFIX" --sysroot="$SYSROOT" --targetos=stanix --enable-static --cc="$HOST-gcc" --triplet=x86_64-stanix \
    --sysincludepaths="/usr/include:/usr/local/include:$PREFIX/lib/tcc/include" \
    --libpaths="/lib:/usr/lib:/usr/local/lib:$PREFIX/lib/tcc" \
    --crtprefix="/usr/lib" \
    --elfinterp="/usr/lib/ld-tlibc.so"
    
	echo '#define CONFIG_TCC_SEMLOCK 0' >> config.h
}

build () {
	make XTCC=gcc XAR="$HOST-ar"
}
