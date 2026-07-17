#!/bin/tash
# script to parse run level scripts

active_list=""
inactive_list=""

active () {
	active_list="$active_list $*"
}

inactive () {
	inactive_list="$inactive_list $*"
}

. ./"$1"

echo "active :$active_list"
echo "inactive :$inactive_list"
