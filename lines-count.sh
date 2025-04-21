#!/bin/sh
#simple tool to count the number of line of code
find -name "*.c" -or -name "*.s" -or -name "*.h" -or -name "*.ld" -or -name "*.mk" -or -name "makefile" -or -name "*.sh"| xargs wc -l