. "$TOP/host-packages/stanix-base.sh"
SOURCE="stanix-userspace"
BUILD_DEPENDENCIES="binutils gcc libgcc"
DEPENDENCIES="tlibc stanix-libraries stanix-modules"
PREFIX=/usr
. "$TOP/scripts/stanix-package.sh"
