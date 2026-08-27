#!/bin/sh

# change this to change the keyboard layout
if test -c ; then
	set-layout /dev/kb* azerty
fi
