VERSION="2.8.2"
SOURCE="sdl2-mixer"
DEPENDENCIES="sdl2-compat"
. "$TOP/scripts/cmake-package.sh"
WEBSITE="https://libsdl.org/"

configure () {
	cmake_configure \
		-DSDL2MIXER_DEPS_SHARED=OFF \
		-DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
		-DSDL2MIXER_WAVPACK=OFF \
		-DSDL2MIXER_OPUS=OFF \
		-DSDL2MIXER_MOD=OFF
}
