# tinx config for stanix
: ${GNU_MIRROR:="https://ftp.gnu.org"}
: ${TAYOKY_MIRROR:="https://github.com/tayoky"}
: ${STANIX_MIRROR:="https://github.com/stanix-org"}
: ${HOST:="$(uname -m)-stanix"}
: ${CFLAGS:="-Wall -Wextra -O2"}
TOOLS_DIR="$TOP/build-packages"
HOST_PACKAGES_DIR="$TOP/host-packages"
