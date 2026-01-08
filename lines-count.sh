#!/bin/sh
#simple tool to count the number of line of code
find kernel/ tlibc/ userspace/ libraries/ ports/git/tash/  ports/git/tutils/  modules/ -name "*.c" -or -name "*.h" | xargs wc -l | sort -n
